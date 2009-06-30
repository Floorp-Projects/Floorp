#ifndef mozilla_tabs_TabTypes_h
#define mozilla_tabs_TabTypes_h

#ifdef XP_WIN
#include <windows.h>

typedef HWND MagicWindowHandle;
#elif defined(MOZ_WIDGET_GTK2)
#include <X11/X.h>

typedef XID MagicWindowHandle;
#else
#error Not implemented, stooge
#endif

#endif
