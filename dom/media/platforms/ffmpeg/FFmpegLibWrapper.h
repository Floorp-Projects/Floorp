/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLibWrapper_h__
#define __FFmpegLibWrapper_h__

#include "FFmpegRDFTTypes.h"  // for AvRdftInitFn, etc.
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVDictionary;
struct AVCodecParserContext;
struct PRLibrary;
#ifdef MOZ_WAYLAND
struct AVCodecHWConfig;
struct AVBufferRef;
#endif

namespace mozilla {

struct MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS FFmpegLibWrapper {
  // The class is used only in static storage and so is zero initialized.
  FFmpegLibWrapper() = default;
  // The libraries are not unloaded in the destructor, because doing so would
  // require a static constructor to register the static destructor.  As the
  // class is in static storage, the destructor would only run on shutdown
  // anyway.
  ~FFmpegLibWrapper() = default;

  enum class LinkResult {
    Success,
    NoProvidedLib,
    NoAVCodecVersion,
    CannotUseLibAV57,
    BlockedOldLibAVVersion,
    UnknownFutureLibAVVersion,
    UnknownFutureFFMpegVersion,
    UnknownOlderFFMpegVersion,
    MissingFFMpegFunction,
    MissingLibAVFunction,
  };
  // Examine mAVCodecLib, mAVUtilLib and mVALib, and attempt to resolve
  // all symbols.
  // Upon failure, the entire object will be reset and any attached libraries
  // will be unlinked.
  LinkResult Link();

  // Reset the wrapper and unlink all attached libraries.
  void Unlink();

#ifdef MOZ_WAYLAND
  // Check if mVALib are available and we can use HW decode.
  bool IsVAAPIAvailable();
  void LinkVAAPILibs();
#endif

  // indicate the version of libavcodec linked to.
  // 0 indicates that the function wasn't initialized with Link().
  int mVersion;

  // libavcodec
  unsigned (*avcodec_version)();
  int (*av_lockmgr_register)(int (*cb)(void** mutex, int op));
  AVCodecContext* (*avcodec_alloc_context3)(const AVCodec* codec);
  int (*avcodec_close)(AVCodecContext* avctx);
  int (*avcodec_decode_audio4)(AVCodecContext* avctx, AVFrame* frame,
                               int* got_frame_ptr, const AVPacket* avpkt);
  int (*avcodec_decode_video2)(AVCodecContext* avctx, AVFrame* picture,
                               int* got_picture_ptr, const AVPacket* avpkt);
  AVCodec* (*avcodec_find_decoder)(int id);
  void (*avcodec_flush_buffers)(AVCodecContext* avctx);
  int (*avcodec_open2)(AVCodecContext* avctx, const AVCodec* codec,
                       AVDictionary** options);
  void (*avcodec_register_all)();
  void (*av_init_packet)(AVPacket* pkt);
  AVCodecParserContext* (*av_parser_init)(int codec_id);
  void (*av_parser_close)(AVCodecParserContext* s);
  int (*av_parser_parse2)(AVCodecParserContext* s, AVCodecContext* avctx,
                          uint8_t** poutbuf, int* poutbuf_size,
                          const uint8_t* buf, int buf_size, int64_t pts,
                          int64_t dts, int64_t pos);

  // only used in libavcodec <= 54
  AVFrame* (*avcodec_alloc_frame)();
  void (*avcodec_get_frame_defaults)(AVFrame* pic);
  // libavcodec v54 only
  void (*avcodec_free_frame)(AVFrame** frame);

  // libavcodec v58 and later only
  int (*avcodec_send_packet)(AVCodecContext* avctx, const AVPacket* avpkt);
  int (*avcodec_receive_frame)(AVCodecContext* avctx, AVFrame* frame);

  // libavcodec optional
  AvRdftInitFn av_rdft_init;
  AvRdftCalcFn av_rdft_calc;
  AvRdftEndFn av_rdft_end;

  // libavutil
  void (*av_log_set_level)(int level);
  void* (*av_malloc)(size_t size);
  void (*av_freep)(void* ptr);

  // libavutil v55 and later only
  AVFrame* (*av_frame_alloc)();
  void (*av_frame_free)(AVFrame** frame);
  void (*av_frame_unref)(AVFrame* frame);

  // libavutil optional
  int (*av_frame_get_colorspace)(const AVFrame* frame);
  int (*av_frame_get_color_range)(const AVFrame* frame);

#ifdef MOZ_WAYLAND
  const AVCodecHWConfig* (*avcodec_get_hw_config)(const AVCodec* codec,
                                                  int index);
  AVBufferRef* (*av_hwdevice_ctx_alloc)(int);
  int (*av_hwdevice_ctx_init)(AVBufferRef* ref);

  AVBufferRef* (*av_buffer_ref)(AVBufferRef* buf);
  void (*av_buffer_unref)(AVBufferRef** buf);
  int (*av_hwframe_transfer_get_formats)(AVBufferRef* hwframe_ctx, int dir,
                                         int** formats, int flags);
  int (*av_hwdevice_ctx_create_derived)(AVBufferRef** dst_ctx, int type,
                                        AVBufferRef* src_ctx, int flags);
  AVBufferRef* (*av_hwframe_ctx_alloc)(AVBufferRef* device_ctx);
  int (*av_dict_set)(AVDictionary** pm, const char* key, const char* value,
                     int flags);
  void (*av_dict_free)(AVDictionary** m);

  int (*vaExportSurfaceHandle)(void*, unsigned int, uint32_t, uint32_t, void*);
  int (*vaSyncSurface)(void*, unsigned int);
  int (*vaInitialize)(void* dpy, int* major_version, int* minor_version);
  int (*vaTerminate)(void* dpy);
  void* (*vaGetDisplayWl)(struct wl_display* display);
  void* (*vaGetDisplayDRM)(int fd);
#endif

  PRLibrary* mAVCodecLib;
  PRLibrary* mAVUtilLib;
#ifdef MOZ_WAYLAND
  PRLibrary* mVALib;
  PRLibrary* mVALibWayland;
  PRLibrary* mVALibDrm;
#endif

 private:
};

}  // namespace mozilla

#endif  // FFmpegLibWrapper
