/*
 * SPDX-License-Identifier: NVIDIA Corporation
 * Copyright (c) 2019-2022.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

# include <stdlib.h>

#include "gstnvdsmeta.h"
//#include "gstnvstreammeta.h"
#ifndef PLATFORM_TEGRA
#include "gst-nvmessage.h"
#endif

#define MAX_DISPLAY_LEN 64

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 4000000

#define TILED_OUTPUT_WIDTH 1920
#define TILED_OUTPUT_HEIGHT 1080

/* NVIDIA Decoder source pad memory feature. This feature signifies that source
 * pads having this capability will push GstBuffers containing cuda buffers. */
#define GST_CAPS_FEATURES_NVMM "memory:NVMM"

//yolo classes
gchar pgie_classes_str[80][32] = {
    "person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light",
    "fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow",
    "elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee",
    "skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle",
    "wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange",
    "broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed",
    "diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
    "toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
};



#define FPS_PRINT_INTERVAL 300
//static struct timeval start_time = { };

//static guint probe_counter = 0;

gint print_counter=0;
gdouble frame_num_1=0;
gdouble frame_num_current=0;
gdouble frame_rate=0;
gdouble time_sec;

int sockfd;
struct sockaddr_in des_addr;


clock_t clock_start;
clock_t clock_end;    
/* tiler_sink_pad_buffer_probe  will extract metadata received on OSD sink pad
 * and update params for drawing rectangle, object information etc. */

static GstPadProbeReturn
tiler_src_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0; 
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList * l_frame = NULL;
    NvDsMetaList * l_obj = NULL;

    int r_flag=1;
    gchar sendline[1024]= {};
    gchar str[25];
    //NvDsDisplayMeta *display_meta = NULL;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta (buf);

    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
      l_frame = l_frame->next) {
        gint  obj_num[80]={};
        float bboxwidth[80]={};
        float bboxheight[80]={};
        float bboxwidth_max[80]={};
        float bboxheight_max[80]={};

        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        //int offset = 0;
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
                l_obj = l_obj->next) {
            guint temp_volume1=0;
            guint temp_volume2=0;

            obj_meta = (NvDsObjectMeta *) (l_obj->data);

            obj_num[obj_meta->class_id]=obj_num[obj_meta->class_id]+1;


            bboxwidth[obj_meta->class_id]=obj_meta->rect_params.width;
            bboxheight[obj_meta->class_id]=obj_meta->rect_params.height;

            temp_volume1=bboxwidth[obj_meta->class_id]*bboxheight[obj_meta->class_id];
            temp_volume2=bboxwidth_max[obj_meta->class_id]*bboxheight_max[obj_meta->class_id];

            if(temp_volume1>temp_volume2)
            {
                bboxwidth_max[obj_meta->class_id]=bboxwidth[obj_meta->class_id];
                bboxheight_max[obj_meta->class_id]=bboxheight[obj_meta->class_id];

            }

            //strcpy(obj_name,pgie_classes_str[obj_meta->class_id]);
            //g_print ("obj_name = %s \n", pgie_classes_str[obj_meta->class_id]);
        }


        print_counter++;
         
        
       if(print_counter>19){
          print_counter=0;
          clock_end  = clock();           
          frame_num_current=frame_meta->frame_num;
          
          time_sec=(double)(clock_end-clock_start)/CLOCKS_PER_SEC;
          frame_rate=(frame_num_current-frame_num_1)/time_sec;


         // g_print ("Frame Number = %d FPS = %d  BBoxWidth= %d BBoxHeight= %d \n", (gint)frame_num_current, (gint)frame_rate, vehicle_count, person_count, bboxwidth, bboxheight);
          g_print ("FPS: %d; ",(gint)frame_rate);


          for (gint i=0; i<80; i++)
          {
             // if(obj_num[i]>0) g_print("%s:%d(W %d H %d); ", pgie_classes_str[i], obj_num[i], bboxwidth_max[i], bboxheight_max[i]);
             if(obj_num[i]>0){
               g_print("%s:%d(%d\%); ", pgie_classes_str[i], obj_num[i], (int)(bboxwidth_max[i]*bboxheight_max[i]*100/1920/1080));

               //strcat(sendline,pgie_classes_str[i]);
               sprintf(str, "%s:%d(%d\%)", pgie_classes_str[i], obj_num[i], (int)(bboxwidth_max[i]*bboxheight_max[i]*100/1920/1080));

               strcat(sendline,str);
             }

          }

          g_print("\n");
          if(strlen(sendline)>1)
          {
              r_flag = sendto(sockfd, sendline, strlen(sendline), 0, (struct sockaddr*)&des_addr, sizeof(des_addr));
                  if (r_flag <= 0)
                  {
                          perror("");
                          exit(-1);
                  }
          }


          frame_num_1=frame_num_current;
          clock_start  = clock();
       }
#if 0
        display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        NvOSD_TextParams *txt_params  = &display_meta->text_params;
        txt_params->display_text = g_malloc0 (MAX_DISPLAY_LEN);
        offset = snprintf(txt_params->display_text, MAX_DISPLAY_LEN, "Person = %d ", person_count);
        offset = snprintf(txt_params->display_text + offset , MAX_DISPLAY_LEN, "Vehicle = %d ", vehicle_count);

        /* Now set the offsets where the string should appear */
        txt_params->x_offset = 10;
        txt_params->y_offset = 12;

        /* Font , font-color and font-size */
        txt_params->font_params.font_name = "Serif";
        txt_params->font_params.font_size = 10;
        txt_params->font_params.font_color.red = 1.0;
        txt_params->font_params.font_color.green = 1.0;
        txt_params->font_params.font_color.blue = 1.0;
        txt_params->font_params.font_color.alpha = 1.0;

        /* Text background color */
        txt_params->set_bg_clr = 1;
        txt_params->text_bg_clr.red = 0.0;
        txt_params->text_bg_clr.green = 0.0;
        txt_params->text_bg_clr.blue = 0.0;
        txt_params->text_bg_clr.alpha = 1.0;

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
#endif

    }
    return GST_PAD_PROBE_OK;
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_WARNING:
    {
      gchar *debug;
      GError *error;
      gst_message_parse_warning (msg, &error, &debug);
      g_printerr ("WARNING from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      g_free (debug);
      g_printerr ("Warning: %s\n", error->message);
      g_error_free (error);
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
#ifndef PLATFORM_TEGRA
    case GST_MESSAGE_ELEMENT:
    {
      if (gst_nvmessage_is_stream_eos (msg)) {
        guint stream_id;
        if (gst_nvmessage_parse_stream_eos (msg, &stream_id)) {
          g_print ("Got EOS from stream %d\n", stream_id);
        }
      }
      break;
    }
#endif
    default:
      break;
  }
  return TRUE;
}

static void
cb_newpad (GstElement * decodebin, GstPad * decoder_src_pad, gpointer data)
{
  g_print ("In cb_newpad\n");
  GstCaps *caps = gst_pad_get_current_caps (decoder_src_pad);
  const GstStructure *str = gst_caps_get_structure (caps, 0);
  const gchar *name = gst_structure_get_name (str);
  GstElement *source_bin = (GstElement *) data;
  GstCapsFeatures *features = gst_caps_get_features (caps, 0);

  /* Need to check if the pad created by the decodebin is for video and not
   * audio. */
  if (!strncmp (name, "video", 5)) {
    /* Link the decodebin pad only if decodebin has picked nvidia
     * decoder plugin nvdec_*. We do this by checking if the pad caps contain
     * NVMM memory features. */
    if (gst_caps_features_contains (features, GST_CAPS_FEATURES_NVMM)) {
      /* Get the source bin ghost pad */
      GstPad *bin_ghost_pad = gst_element_get_static_pad (source_bin, "src");
      if (!gst_ghost_pad_set_target (GST_GHOST_PAD (bin_ghost_pad),
              decoder_src_pad)) {
        g_printerr ("Failed to link decoder src pad to source bin ghost pad\n");
      }
      gst_object_unref (bin_ghost_pad);
    } else {
      g_printerr ("Error: Decodebin did not pick nvidia decoder plugin.\n");
    }
  }
}

static void
decodebin_child_added (GstChildProxy * child_proxy, GObject * object,
    gchar * name, gpointer user_data)
{
  g_print ("Decodebin child added: %s\n", name);
  if (g_strrstr (name, "decodebin") == name) {
    g_signal_connect (G_OBJECT (object), "child-added",
        G_CALLBACK (decodebin_child_added), user_data);
  }
  if (g_strstr_len (name, -1, "nvv4l2decoder") == name) {
    g_print ("Seting bufapi_version\n");
    g_object_set (object, "bufapi-version", TRUE, NULL);
  }
}

static GstElement *
create_source_bin (guint index, gchar * uri)
{
  GstElement *bin = NULL, *uri_decode_bin = NULL;
  gchar bin_name[16] = { };

  g_snprintf (bin_name, 15, "source-bin-%02d", index);
  /* Create a source GstBin to abstract this bin's content from the rest of the
   * pipeline */
  bin = gst_bin_new (bin_name);

  /* Source element for reading from the uri.
   * We will use decodebin and let it figure out the container format of the
   * stream and the codec and plug the appropriate demux and decode plugins. */
  uri_decode_bin = gst_element_factory_make ("uridecodebin", "uri-decode-bin");

  if (!bin || !uri_decode_bin) {
    g_printerr ("One element in source bin could not be created.\n");
    return NULL;
  }

  /* We set the input uri to the source element */
  g_object_set (G_OBJECT (uri_decode_bin), "uri", uri, NULL);

  /* Connect to the "pad-added" signal of the decodebin which generates a
   * callback once a new pad for raw data has beed created by the decodebin */
  g_signal_connect (G_OBJECT (uri_decode_bin), "pad-added",
      G_CALLBACK (cb_newpad), bin);
  g_signal_connect (G_OBJECT (uri_decode_bin), "child-added",
      G_CALLBACK (decodebin_child_added), bin);

  gst_bin_add (GST_BIN (bin), uri_decode_bin);

  /* We need to create a ghost pad for the source bin which will act as a proxy
   * for the video decoder src pad. The ghost pad will not have a target right
   * now. Once the decode bin creates the video decoder and generates the
   * cb_newpad callback, we will set the ghost pad target to the video decoder
   * src pad. */
  if (!gst_element_add_pad (bin, gst_ghost_pad_new_no_target ("src",
              GST_PAD_SRC))) {
    g_printerr ("Failed to add ghost pad in source bin\n");
    return NULL;
  }

  return bin;
}


static GstElement *
create_camera_source_bin (guint index, gchar * uri)
{
  GstElement *bin = NULL;
  GstCaps *caps_RGBA_NVMM = NULL;
  GstCaps *caps_bayer = NULL;
  GstCaps *caps_RGBA = NULL;
  GstCaps *caps1 = NULL;
  gboolean ret = FALSE;

  gchar bin_name[16] = { };
  g_snprintf (bin_name, 15, "source-bin-%02d", index);
  bin = gst_bin_new (bin_name);

  //create a souce for ARAVIS SDK
  GstElement *src_elem = gst_element_factory_make ("aravissrc", "src_elem");
  if (!src_elem)
  {
    g_printerr ("Could not create 'src_elem'\n");
    return NULL;
  }

  GstElement *bayer2rgb_elem= gst_element_factory_make("bayer2rgb", "bayer2rgb_elem");
  if (!bayer2rgb_elem)
  {
    g_printerr ("Could not create 'bayer2rgb_elem'\n");
    return NULL;
  }

  GstElement *cap_filter = gst_element_factory_make ("capsfilter", "src_cap_filter");
  if (!cap_filter){
    g_printerr ("Could not create 'src_cap_filter'\n");
    return NULL;
  }

  caps_RGBA_NVMM = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA", 
		  "width", G_TYPE_INT, 1920, "height", G_TYPE_INT, 1080, 
		  "framerate", GST_TYPE_FRACTION, 40, 1, NULL);

  caps_RGBA = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA", 
		  "width", G_TYPE_INT, 1920, "height", G_TYPE_INT, 1080, 
		  "framerate", GST_TYPE_FRACTION, 40, 1, NULL);

  caps_bayer = gst_caps_new_simple("video/x-bayer", "width", G_TYPE_INT, 1920, "height", G_TYPE_INT, 1080, 
		  "framerate", GST_TYPE_FRACTION, 40, 1, NULL);

  GstElement *nvvidconv1, *nvvidconv2;
  GstCapsFeatures *feature = NULL;

  nvvidconv1 = gst_element_factory_make ("videoconvert", "nvvidconv1");
  if (!nvvidconv1){
    g_printerr ("Failed to create 'nvvidcovn1'\n");
    return NULL;
  }

 feature = gst_caps_features_new ("memory:NVMM", NULL);
 gst_caps_set_features (caps_RGBA_NVMM, 0, feature);
  g_object_set (G_OBJECT (cap_filter), "caps", caps_RGBA_NVMM, NULL);

  nvvidconv2 = gst_element_factory_make ("nvvideoconvert", "nvvidconv2");
  if (!nvvidconv2){char sendline[1024] = {"HelloWJ"};
    g_printerr ("Failed to create 'nvvidcovn2'\n");
    return NULL;
  }

  //important to use the memory type: 3
  g_object_set (G_OBJECT (nvvidconv2), "gpu-id", 0, "nvbuf-memory-type", 3, NULL);

 
  gst_bin_add_many (GST_BIN (bin), src_elem, bayer2rgb_elem, nvvidconv1, nvvidconv2, cap_filter, NULL);

  
  //NVGSTDS_LINK_ELEMENT
  if (!gst_element_link_filtered(src_elem, bayer2rgb_elem, caps_bayer)){
    g_printerr ("Failed to link 'src_elem, bayer2rgb_elem'\n");
    return NULL;
  }

  if (!gst_element_link(bayer2rgb_elem, nvvidconv1)){
    g_printerr ("Failed to link 'bayer2rgb_elem, nvvidcovn1'\n");
    return NULL;
  }
  if (!gst_element_link_filtered(nvvidconv1,nvvidconv2,caps_RGBA)){
    g_printerr ("Failed to link 'nvvidcovn1, nvvidcovn2'\n");
    return NULL;
  }
  if (!gst_element_link (nvvidconv2, cap_filter)){
    g_printerr ("Failed to link 'nvvidcovn2, cap_filter'\n");
    return NULL;
  }

  // NVGSTDS_BIN_ADD_GHOST_PAD (bin, cap_filter, "src");
  GstPad *gstpad = gst_element_get_static_pad (cap_filter, "src");
  gst_element_add_pad (bin, gst_ghost_pad_new ("src", gstpad));
  gst_object_unref(gstpad);

  ////gchar device[64];
  ////g_snprintf (device, sizeof (device), "/dev/video%d", 0);
  ////g_object_set (G_OBJECT (src_elem), "device", device, NULL);

  return bin;
}



int
main (int argc, char *argv[])
{
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL, *streammux = NULL, *sink = NULL, *pgie = NULL, *nvosd = NULL, *nvvidconv = NULL, *nvvidconv1 = NULL, *enc = NULL;
#ifdef PLATFORM_TEGRA
  GstElement *transform = NULL;
#endif
  GstBus *bus = NULL;
  guint bus_watch_id;
  GstPad *tiler_src_pad = NULL;
  guint i, num_sources;
  guint tiler_rows, tiler_columns;
  guint pgie_batch_size;
  char file_name[100];
  guint with_default = 1;
  
  const int on = 1;

  /* Check input arguments */
  if (argc == 2) {
     strcpy(file_name, argv[1]);
     with_default = 0;  //otherwise it goes video-render
  }


  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)); 
  bzero(&des_addr, sizeof(des_addr));
  des_addr.sin_family = AF_INET;
  des_addr.sin_addr.s_addr = inet_addr("192.168.8.255"); 
  des_addr.sin_port = htons(9999);


  ///num_sources = argc - 1;
  num_sources=1;

  /* Standard GStreamer initialization */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  /* Create Pipeline element that will form a connection of other elements */
  pipeline = gst_pipeline_new ("gige-pipeline");

  /* Create nvstreammux instance to form batches from one or more sources. */
  streammux = gst_element_factory_make ("nvstreammux", "stream-muxer");

  if (!pipeline || !streammux) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }
  gst_bin_add (GST_BIN (pipeline), streammux);

  for (i = 0; i < num_sources; i++) {
    GstPad *sinkpad, *srcpad;
    gchar pad_name[16] = { };
    //// GstElement *source_bin = create_source_bin (i, argv[i + 1]);
    GstElement *source_bin = create_camera_source_bin (i, argv[i + 1]);


    if (!source_bin) {
      g_printerr ("Failed to create source bin. Exiting.\n");
      return -1;
    }

    gst_bin_add (GST_BIN (pipeline), source_bin);

    g_snprintf (pad_name, 15, "sink_%u", i);
    sinkpad = gst_element_get_request_pad (streammux, pad_name);
    if (!sinkpad) {
      g_printerr ("Streammux request sink pad failed. Exiting.\n");
      return -1;
    }

    srcpad = gst_element_get_static_pad (source_bin, "src");
    if (!srcpad) {
      g_printerr ("Failed to get src pad of source bin. Exiting.\n");
      return -1;
    }

    if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link source bin to stream muxer. Exiting.\n");
      return -1;
    }

    gst_object_unref (srcpad);
    gst_object_unref (sinkpad);
  }

  /* Use nvinfer to infer on batched frame. */
  pgie = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");

  /* Use nvtiler to composite the batched frames into a 2D tiled array based
   * on the source of the frames. */
  //tiler = gst_element_factory_make ("nvmultistreamtiler", "nvtiler");

  /* Use convertor to convert from NV12 to RGBA as required by dsexample */
  nvvidconv = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter");
  nvvidconv1 = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter1");

  /* Create OSD to draw on the converted RGBA buffer */
  nvosd = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");

  /* Finally render the osd output */
#ifdef PLATFORM_TEGRA
  transform = gst_element_factory_make ("nvegltransform", "nvegl-transform");
#endif

  //save the entire video stream: nvvideoconvert ! jpegenc ! filesink
  if(!with_default) {
     sink = gst_element_factory_make ("filesink", "file-sink");
     enc = gst_element_factory_make ("nvv4l2h264enc", "h264-enc");
     g_object_set (G_OBJECT (sink), "location", file_name, NULL);
     //g_object_set (G_OBJECT (sink), "location", "./out.h264", NULL);
  }else {
     sink = gst_element_factory_make ("nveglglessink", "nvvideo-renderer");
     g_object_set (G_OBJECT(sink), "sync", FALSE, NULL);
  }


  if (!pgie || !nvvidconv || !nvosd || !sink ) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

#ifdef PLATFORM_TEGRA
  if(!transform) {
    g_printerr ("One tegra element could not be created. Exiting.\n");
    return -1;
  }
#endif

  //use the meory type: 3
  g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH, "height",
      MUXER_OUTPUT_HEIGHT, "batch-size", num_sources, "live-source", 1,
      "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, "gpu-id", 0, "nvbuf-memory-type", 3, NULL);

  g_object_set (G_OBJECT (nvvidconv), "nvbuf-memory-type", 3, NULL);
   
  //g_object_set (G_OBJECT (streammux), "width", MUXER_OUTPUT_WIDTH, "height",
  //    MUXER_OUTPUT_HEIGHT, "batch-size", num_sources, "live-source", 1,
  //    "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);

  /* Configure the nvinfer element using the nvinfer config file. */
  g_object_set (G_OBJECT (pgie),
      "config-file-path", "config_infer_primary_yoloV3.txt", NULL);

  /* Override the batch-size set in the config file with the number of sources. */
  g_object_get (G_OBJECT (pgie), "batch-size", &pgie_batch_size, NULL);
  if (pgie_batch_size != num_sources) {
    g_printerr
        ("WARNING: Overriding infer-config batch-size (%d) with number of sources (%d)\n",
        pgie_batch_size, num_sources);
    g_object_set (G_OBJECT (pgie), "batch-size", num_sources, NULL);
  }

  //tiler_rows = (guint) sqrt (num_sources);
  //tiler_columns = (guint) ceil (1.0 * num_sources / tiler_rows);
  /* we set the tiler properties here */
  //g_object_set (G_OBJECT (tiler), "rows", tiler_rows, "columns", tiler_columns,
  //    "width", TILED_OUTPUT_WIDTH, "height", TILED_OUTPUT_HEIGHT, NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* Set up the pipeline */
  /* we add all elements into the pipeline */
#ifdef PLATFORM_TEGRA
  gst_bin_add_many (GST_BIN (pipeline), pgie, nvosd, transform, sink, NULL);
  /* we link the elements together
   * nvstreammux -> nvinfer -> nvosd -> video-renderer */
  if (!gst_element_link_many (streammux, pgie,  nvosd, transform, sink, NULL)) {
    g_printerr ("Elements could not be linked. Exiting.\n");
    return -1;
  }
#else

  if (!with_default){
     //save file: with “nvvideoconvert ! jpegenc ! filesink” 
     gst_bin_add_many (GST_BIN (pipeline), pgie, nvvidconv, nvosd, nvvidconv1, enc, sink, NULL);
     /* we link the elements together
      * nvstreammux -> nvinfer ->  nvosd -> nvvidcov1 -> enc -> sink  */
     if (!gst_element_link_many (streammux, pgie, nvvidconv, nvosd, nvvidconv1, enc, sink, NULL)) {
       g_printerr ("Elements could not be linked. Exiting.\n");
       return -1;
     }
  }else{
     /* we link the elements together
      * nvstreammux -> nvinfer ->  nvosd -> video-renderer */
     gst_bin_add_many (GST_BIN (pipeline), pgie, nvvidconv, nvosd, sink, NULL);
     if (!gst_element_link_many (streammux, pgie, nvvidconv, nvosd, sink, NULL)) {
       g_printerr ("Elements could not be linked. Exiting.\n");
       return -1;
     }
  }
#endif

  /* Lets add probe to get informed of the meta data generated, we add probe to
   * the sink pad of the osd element, since by that time, the buffer would have
   * had got all the metadata. */
  tiler_src_pad = gst_element_get_static_pad (pgie, "src");
  //tiler_src_pad = gst_element_get_static_pad (nvosd, "src");
  if (!tiler_src_pad)
    g_print ("Unable to get src pad\n");
  else
    gst_pad_add_probe (tiler_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
        tiler_src_pad_buffer_probe, NULL, NULL);

  /* Set the pipeline to "playing" state */
  g_print ("Now playing:");
 // for (i = 0; i < num_sources; i++) {
 //   g_print (" %s,", argv[i + 1]);
 // }
  g_print ("\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);




  /* Wait till pipeline encounters an error or EOS */
  g_print ("Running...\n");
  clock_start  = clock();
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref(loop);
  return 0;
}
