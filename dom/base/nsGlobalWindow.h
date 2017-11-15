/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindow_h___
#define nsGlobalWindow_h___

// XXX(nika): Figure out where to put this?
#define DEFAULT_HOME_PAGE "www.mozilla.org"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

// Amount of time allowed between alert/prompt/confirm before enabling
// the stop dialog checkbox.
#define DEFAULT_SUCCESSIVE_DIALOG_TIME_LIMIT 3 // 3 sec

// Maximum number of successive dialogs before we prompt users to disable
// dialogs for this window.
#define MAX_SUCCESSIVE_DIALOG_COUNT 5

// Idle fuzz time upper limit
#define MAX_IDLE_FUZZ_TIME_MS 90000

// Min idle notification time in seconds.
#define MIN_IDLE_NOTIFICATION_TIME_S 1

// NOTE: This is so that I can rewrite the includes in a separate patch.
// Specificially I don't think I want to change this until I've moved everything
// to mozilla/dom/Window.h and mozilla/dom/WindowProxy.h.

#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"

#endif /* nsGlobalWindow_h___ */
