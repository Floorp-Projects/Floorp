/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  NS_MIME_EMITTER_CONTRACTID_PREFIX "application/vnd.mozilla.xul+xml"
// {FAA8AF16-DCFF-11d2-A411-00805F613C19}
#define NS_XUL_MIME_EMITTER_CID   \
    { 0xfaa8af16, 0xdcff, 0x11d2,         \
    { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x19 } }

#endif // nsMimeEmitterCID_h__
