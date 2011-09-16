#include "android_npapi.h"
#include <stdlib.h>
#include "nsAutoPtr.h"
#include "gfxFont.h"

#define NOT_IMPLEMENTED_FATAL() do {                                    \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "%s not implemented %s, %d",                    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \
    abort();                                                            \
  } while(0)

#define NOT_IMPLEMENTED()                                               \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "!!!!!!!!!!!!!!  %s not implemented %s, %d",    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \

class gfxFont;

void InitAudioTrackInterface(ANPAudioTrackInterfaceV0 *i);
void InitBitmapInterface(ANPBitmapInterfaceV0 *i);
void InitCanvasInterface(ANPCanvasInterfaceV0 *i);
void InitEventInterface(ANPEventInterfaceV0 *i);
void InitLogInterface(ANPLogInterfaceV0 *i);
void InitMatrixInterface(ANPMatrixInterfaceV0 *i);
void InitPaintInterface(ANPPaintInterfaceV0 *i);
void InitPathInterface(ANPPathInterfaceV0 *i);
void InitSurfaceInterface(ANPSurfaceInterfaceV0 *i);
void InitSystemInterface(ANPSystemInterfaceV0 *i);
void InitTypeFaceInterface(ANPTypefaceInterfaceV0 *i);
void InitWindowInterface(ANPWindowInterfaceV0 *i);

struct ANPTypeface {
  nsRefPtr<gfxFont> mFont;
};


typedef struct {
  ANPMatrixFlag flags;
  ANPColor color;
  ANPPaintStyle style;
  float strokeWidth;
  float strokeMiter;
  ANPPaintCap paintCap;
  ANPPaintJoin paintJoin;
  ANPTextEncoding textEncoding;
  ANPPaintAlign paintAlign;
  float textSize;
  float textScaleX;
  float textSkewX;
  ANPTypeface typeface;
} ANPPaintPrivate;
