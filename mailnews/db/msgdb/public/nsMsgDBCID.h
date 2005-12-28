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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsMsgDBCID_h__
#define nsMsgDBCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

// 03223c50-1e88-45e8-ba1a-7ce792dc3fc3
#define NS_MSGDB_SERVICE_CID \
{  0x03223c50, 0x1e88, 0x45e8, \
    { 0xba, 0x1a, 0x7c, 0xe7, 0x92, 0xdc, 0x3f, 0xc3 } }

#define NS_MSGDB_SERVICE_CONTRACTID \
  "@mozilla.org/msgDatabase/msgDBService;1"

#define NS_MSGDB_CONTRACTID \
  "@mozilla.org/nsMsgDatabase/msgDB-"

#define NS_MAILBOXDB_CONTRACTID \
  NS_MSGDB_CONTRACTID"mailbox"

//	a86c86ae-e97f-11d2-a506-0060b0fc04b7
#define NS_MAILDB_CID                      \
{ 0xa86c86ae, 0xe97f, 0x11d2,                   \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

#define NS_NEWSDB_CONTRACTID \
  NS_MSGDB_CONTRACTID"news"

// 36414aa0-e980-11d2-a506-0060b0fc04b7
#define NS_NEWSDB_CID                      \
{ 0x36414aa0, 0xe980, 0x11d2,                  \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

#define NS_IMAPDB_CONTRACTID \
  NS_MSGDB_CONTRACTID"imap"

// 9e4b07ee-e980-11d2-a506-0060b0fc04b7
#define NS_IMAPDB_CID                      \
{ 0x9e4b07ee, 0xe980, 0x11d2,                  \
    { 0xa5, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

#define NS_MSG_RETENTIONSETTINGS_CID \
{ 0x1bd976d6, 0xdf44, 0x11d4,       \
  {0xa5, 0xb6, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7} }

#define NS_MSG_RETENTIONSETTINGS_CONTRACTID \
  "@mozilla.org/msgDatabase/retentionSettings;1"

// 4e3dae5a-157a-11d5-a5c0-0060b0fc04b7
#define NS_MSG_DOWNLOADSETTINGS_CID \
{ 0x4e3dae5a, 0x157a, 0x11d5,       \
  {0xa5, 0xc0, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7} }

#define NS_MSG_DOWNLOADSETTINGS_CONTRACTID \
  "@mozilla.org/msgDatabase/downloadSettings;1"

#endif
