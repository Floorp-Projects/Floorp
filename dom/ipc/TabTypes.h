/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */

#ifndef mozilla_tabs_TabTypes_h
#define mozilla_tabs_TabTypes_h

#include "base/basictypes.h"

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
