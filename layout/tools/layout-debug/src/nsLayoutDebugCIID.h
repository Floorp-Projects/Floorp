/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameDebugCIID_h__
#define nsFrameDebugCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_REGRESSION_TESTER_CID \
{ 0x698c54f4, 0x4ea9, 0x11d7, \
{ 0x85, 0x9f, 0x00, 0x03, 0x93, 0x63, 0x65, 0x92 } }

#define NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID \
  "@mozilla.org/layout-debug/layout-debuggingtools;1"
// 3f4c3b63-e640-4712-abbf-fff1301ceb60
#define NS_LAYOUT_DEBUGGINGTOOLS_CID { 0x3f4c3b68, 0xe640, 0x4712, \
    { 0xab, 0xbf, 0xff, 0xf1, 0x30, 0x1c, 0xeb, 0x60}}

#endif // nsFrameDebugCIID_h__

