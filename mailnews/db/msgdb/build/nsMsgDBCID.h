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

#ifndef nsMsgDBCID_h__
#define nsMsgDBCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

//	a86c86ae-e97f-11d2-a506-0060b0fc04b7
#define NS_MAILDB_CID                      \
{ 0xa86c86ae, 0xe97f, 0x11d2,                   \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// 36414aa0-e980-11d2-a506-0060b0fc04b7
#define NS_NEWSDB_CID                      \
{ 0x36414aa0, 0xe980, 0x11d2,                  \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// 9e4b07ee-e980-11d2-a506-0060b0fc04b7
#define NS_IMAPDB_CID                      \
{ 0x9e4b07ee, 0xe980, 0x11d2,                  \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

#define NS_MSG_RETENTIONSETTINGS_CID \
{ 0x1bd976d6, 0xdf44, 0x11d4,       \
  {0xa5, 0xb6, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7} }

#define NS_MSG_RETENTIONSETTINGS_CONTRACTID \
  "@mozilla.org/msgDatabase/retentionSettings;1"
#endif
