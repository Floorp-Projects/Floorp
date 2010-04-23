/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

