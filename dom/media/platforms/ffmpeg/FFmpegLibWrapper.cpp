/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegLibWrapper.h"
#include "FFmpegLog.h"
#include "mozilla/PodOperations.h"
#ifdef MOZ_FFMPEG
#  include "mozilla/StaticPrefs_media.h"
#endif
#include "mozilla/Types.h"
#include "PlatformDecoderModule.h"
#include "prlink.h"

#define AV_LOG_DEBUG 48
#define AV_LOG_INFO 32

namespace mozilla {

FFmpegLibWrapper::LinkResult FFmpegLibWrapper::Link() {
  if (!mAVCodecLib || !mAVUtilLib) {
    Unlink();
    return LinkResult::NoProvidedLib;
  }

  avcodec_version =
      (decltype(avcodec_version))PR_FindSymbol(mAVCodecLib, "avcodec_version");
  if (!avcodec_version) {
    Unlink();
    return LinkResult::NoAVCodecVersion;
  }
  uint32_t version = avcodec_version();
  uint32_t macro = (version >> 16) & 0xFFu;
  mVersion = static_cast<int>(macro);
  uint32_t micro = version & 0xFFu;
  // A micro version >= 100 indicates that it's FFmpeg (as opposed to LibAV).
  bool isFFMpeg = micro >= 100;
  if (!isFFMpeg) {
    if (macro == 57) {
      // Due to current AVCodecContext binary incompatibility we can only
      // support FFmpeg 57 at this stage.
      Unlink();
      return LinkResult::CannotUseLibAV57;
    }
#ifdef MOZ_FFMPEG
    if (version < (54u << 16 | 35u << 8 | 1u) &&
        !StaticPrefs::media_libavcodec_allow_obsolete()) {
      // Refuse any libavcodec version prior to 54.35.1.
      // (Unless media.libavcodec.allow-obsolete==true)
      Unlink();
      return LinkResult::BlockedOldLibAVVersion;
    }
#endif
  }

  enum {
    AV_FUNC_AVUTIL_MASK = 1 << 8,
    AV_FUNC_53 = 1 << 0,
    AV_FUNC_54 = 1 << 1,
    AV_FUNC_55 = 1 << 2,
    AV_FUNC_56 = 1 << 3,
    AV_FUNC_57 = 1 << 4,
    AV_FUNC_58 = 1 << 5,
    AV_FUNC_AVUTIL_53 = AV_FUNC_53 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVUTIL_54 = AV_FUNC_54 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVUTIL_55 = AV_FUNC_55 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVUTIL_56 = AV_FUNC_56 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVUTIL_57 = AV_FUNC_57 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVUTIL_58 = AV_FUNC_58 | AV_FUNC_AVUTIL_MASK,
    AV_FUNC_AVCODEC_ALL = AV_FUNC_53 | AV_FUNC_54 | AV_FUNC_55 | AV_FUNC_56 |
                          AV_FUNC_57 | AV_FUNC_58,
    AV_FUNC_AVUTIL_ALL = AV_FUNC_AVCODEC_ALL | AV_FUNC_AVUTIL_MASK
  };

  switch (macro) {
    case 53:
      version = AV_FUNC_53;
      break;
    case 54:
      version = AV_FUNC_54;
      break;
    case 55:
      version = AV_FUNC_55;
      break;
    case 56:
      version = AV_FUNC_56;
      break;
    case 57:
      version = AV_FUNC_57;
      break;
    case 58:
      version = AV_FUNC_58;
      break;
    default:
      FFMPEG_LOG("Unknown avcodec version");
      Unlink();
      return isFFMpeg ? ((macro > 57) ? LinkResult::UnknownFutureFFMpegVersion
                                      : LinkResult::UnknownOlderFFMpegVersion)
                      // All LibAV versions<54.35.1 are blocked, therefore we
                      // must be dealing with a later one.
                      : LinkResult::UnknownFutureLibAVVersion;
  }

#define AV_FUNC_OPTION_SILENT(func, ver)                              \
  if ((ver)&version) {                                                \
    if (!(func = (decltype(func))PR_FindSymbol(                       \
              ((ver)&AV_FUNC_AVUTIL_MASK) ? mAVUtilLib : mAVCodecLib, \
              #func))) {                                              \
    }                                                                 \
  } else {                                                            \
    func = (decltype(func)) nullptr;                                  \
  }

#define AV_FUNC_OPTION(func, ver)                            \
  AV_FUNC_OPTION_SILENT(func, ver)                           \
  if ((ver)&version && (func) == (decltype(func)) nullptr) { \
    FFMPEG_LOG("Couldn't load function " #func);             \
  }

#define AV_FUNC(func, ver)                              \
  AV_FUNC_OPTION(func, ver)                             \
  if ((ver)&version && !func) {                         \
    Unlink();                                           \
    return isFFMpeg ? LinkResult::MissingFFMpegFunction \
                    : LinkResult::MissingLibAVFunction; \
  }

  AV_FUNC(av_lockmgr_register, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_alloc_context3, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_close, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_decode_audio4, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_decode_video2, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_find_decoder, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_flush_buffers, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_open2, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_register_all, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(av_init_packet, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(av_parser_init, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(av_parser_close, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(av_parser_parse2, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(avcodec_alloc_frame, (AV_FUNC_53 | AV_FUNC_54))
  AV_FUNC(avcodec_get_frame_defaults, (AV_FUNC_53 | AV_FUNC_54))
  AV_FUNC(avcodec_free_frame, AV_FUNC_54)
  AV_FUNC(avcodec_send_packet, AV_FUNC_58)
  AV_FUNC(avcodec_receive_frame, AV_FUNC_58)
  AV_FUNC_OPTION(av_rdft_init, AV_FUNC_AVCODEC_ALL)
  AV_FUNC_OPTION(av_rdft_calc, AV_FUNC_AVCODEC_ALL)
  AV_FUNC_OPTION(av_rdft_end, AV_FUNC_AVCODEC_ALL)
  AV_FUNC(av_log_set_level, AV_FUNC_AVUTIL_ALL)
  AV_FUNC(av_malloc, AV_FUNC_AVUTIL_ALL)
  AV_FUNC(av_freep, AV_FUNC_AVUTIL_ALL)
  AV_FUNC(av_frame_alloc, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 |
                           AV_FUNC_AVUTIL_57 | AV_FUNC_AVUTIL_58))
  AV_FUNC(av_frame_free, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 |
                          AV_FUNC_AVUTIL_57 | AV_FUNC_AVUTIL_58))
  AV_FUNC(av_frame_unref, (AV_FUNC_AVUTIL_55 | AV_FUNC_AVUTIL_56 |
                           AV_FUNC_AVUTIL_57 | AV_FUNC_AVUTIL_58))
  AV_FUNC_OPTION(av_frame_get_colorspace, AV_FUNC_AVUTIL_ALL)
  AV_FUNC_OPTION(av_frame_get_color_range, AV_FUNC_AVUTIL_ALL)
#ifdef MOZ_WAYLAND
  AV_FUNC_OPTION_SILENT(avcodec_get_hw_config, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_hwdevice_ctx_create, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_buffer_ref, AV_FUNC_AVUTIL_58)
  AV_FUNC_OPTION_SILENT(av_buffer_unref, AV_FUNC_AVUTIL_58)
  AV_FUNC_OPTION_SILENT(av_hwframe_transfer_get_formats, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_hwdevice_ctx_create_derived, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_hwframe_ctx_alloc, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_dict_set, AV_FUNC_58)
  AV_FUNC_OPTION_SILENT(av_dict_free, AV_FUNC_58)
#endif
#undef AV_FUNC
#undef AV_FUNC_OPTION

#ifdef MOZ_WAYLAND
#  define VA_FUNC_OPTION_SILENT(func)                             \
    if (!(func = (decltype(func))PR_FindSymbol(mVALib, #func))) { \
      func = (decltype(func)) nullptr;                            \
    }

  // mVALib is optional and may not be present.
  if (mVALib) {
    VA_FUNC_OPTION_SILENT(vaExportSurfaceHandle)
    VA_FUNC_OPTION_SILENT(vaSyncSurface)
  }
#  undef VA_FUNC_OPTION
#endif

  avcodec_register_all();
  if (MOZ_LOG_TEST(sPDMLog, LogLevel::Debug)) {
    av_log_set_level(AV_LOG_DEBUG);
  } else if (MOZ_LOG_TEST(sPDMLog, LogLevel::Info)) {
    av_log_set_level(AV_LOG_INFO);
  } else {
    av_log_set_level(0);
  }
  return LinkResult::Success;
}

void FFmpegLibWrapper::Unlink() {
  if (av_lockmgr_register) {
    // Registering a null lockmgr cause the destruction of libav* global mutexes
    // as the default lockmgr that allocated them will be deregistered.
    // This prevents ASAN and valgrind to report sizeof(pthread_mutex_t) leaks.
    av_lockmgr_register(nullptr);
  }
#ifndef MOZ_TSAN
  // With TSan, we cannot unload libav once we have loaded it because
  // TSan does not support unloading libraries that are matched from its
  // suppression list. Hence we just keep the library loaded in TSan builds.
  if (mAVUtilLib && mAVUtilLib != mAVCodecLib) {
    PR_UnloadLibrary(mAVUtilLib);
  }
  if (mAVCodecLib) {
    PR_UnloadLibrary(mAVCodecLib);
  }
#endif
#ifdef MOZ_WAYLAND
  if (mVALib) {
    PR_UnloadLibrary(mVALib);
  }
#endif
  PodZero(this);
}

#ifdef MOZ_WAYLAND
bool FFmpegLibWrapper::IsVAAPIAvailable() {
#  define VA_FUNC_LOADED(func) (func != nullptr)
  return VA_FUNC_LOADED(avcodec_get_hw_config) &&
         VA_FUNC_LOADED(av_hwdevice_ctx_create) &&
         VA_FUNC_LOADED(av_buffer_ref) && VA_FUNC_LOADED(av_buffer_unref) &&
         VA_FUNC_LOADED(av_hwframe_transfer_get_formats) &&
         VA_FUNC_LOADED(av_hwdevice_ctx_create_derived) &&
         VA_FUNC_LOADED(av_hwframe_ctx_alloc) && VA_FUNC_LOADED(av_dict_set) &&
         VA_FUNC_LOADED(av_dict_free) &&
         VA_FUNC_LOADED(vaExportSurfaceHandle) && VA_FUNC_LOADED(vaSyncSurface);
}
#endif

}  // namespace mozilla
