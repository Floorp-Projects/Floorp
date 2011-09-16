#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_window_##name

void
anp_window_setVisibleRects(NPP instance, const ANPRectI rects[], int32_t count)
{
  NOT_IMPLEMENTED();
}

void
anp_window_clearVisibleRects(NPP instance)
{
  NOT_IMPLEMENTED();
}

void
anp_window_showKeyboard(NPP instance, bool value)
{
  NOT_IMPLEMENTED();
}

void
anp_window_requestFullScreen(NPP instance)
{
  NOT_IMPLEMENTED();
}

void
anp_window_exitFullScreen(NPP instance)
{
  NOT_IMPLEMENTED();
}

void
anp_window_requestCenterFitZoom(NPP instance)
{
  NOT_IMPLEMENTED();
}

void InitWindowInterface(ANPWindowInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, setVisibleRects);
  ASSIGN(i, clearVisibleRects);
  ASSIGN(i, showKeyboard);
  ASSIGN(i, requestFullScreen);
  ASSIGN(i, exitFullScreen);
  ASSIGN(i, requestCenterFitZoom);
}

