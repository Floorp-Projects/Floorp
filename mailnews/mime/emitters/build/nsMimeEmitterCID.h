/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMimeEmitterCID_h__
#define nsMimeEmitterCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_MIME_EMITTER_CONTRACTID_PREFIX \
  "@mozilla.org/messenger/mimeemitter;1?type="

#define NS_HTML_MIME_EMITTER_CONTRACTID   \
  NS_MIME_EMITTER_CONTRACTID_PREFIX "text/html"
// {F0A8AF16-DCCE-11d2-A411-00805F613C79}
#define NS_HTML_MIME_EMITTER_CID   \
    { 0xf0a8af16, 0xdcce, 0x11d2,         \
    { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x79 } }

#define NS_XML_MIME_EMITTER_CONTRACTID   \
  NS_MIME_EMITTER_CONTRACTID_PREFIX "text/xml"
// {977E418F-E392-11d2-A2AC-00A024A7D144}
#define NS_XML_MIME_EMITTER_CID   \
    { 0x977e418f, 0xe392, 0x11d2, \
    { 0xa2, 0xac, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_RAW_MIME_EMITTER_CONTRACTID   \
  NS_MIME_EMITTER_CONTRACTID_PREFIX "raw"
// {F0A8AF16-DCFF-11d2-A411-00805F613C79}
#define NS_RAW_MIME_EMITTER_CID   \
    { 0xf0a8af16, 0xdcff, 0x11d2,         \
    { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x79 } }

#define NS_XUL_MIME_EMITTER_CONTRACTID   \
  NS_MIME_EMITTER_CONTRACTID_PREFIX "text/xul"
// {FAA8AF16-DCFF-11d2-A411-00805F613C19}
#define NS_XUL_MIME_EMITTER_CID   \
    { 0xfaa8af16, 0xdcff, 0x11d2,         \
    { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x19 } }

#endif // nsMimeEmitterCID_h__
