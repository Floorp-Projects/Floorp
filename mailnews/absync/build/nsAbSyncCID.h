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

#ifndef nsAbSyncCID_h__
#define nsAbSyncCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

//
// Ab Sync Service
//
#define NS_ABSYNC_SERVICE_CID				  \
{ /* {3C21BB39-0A87-11d4-8FD6-00A024A7D144} */      \
    0x3c21bb39, 0xa87, 0x11d4,                      \
  { 0x8f, 0xd6, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_ABSYNC_SERVICE_CONTRACTID			\
  "@mozilla.org/absync;1"

//
// Ab Sync Listener
//
#define NS_ABSYNC_LISTENER_CID				  \
{ /* {3C21BB44-0A87-11d4-8FD6-00A024A7D144} */      \
    0x3c21bb44, 0xa87, 0x11d4,                      \
  { 0x8f, 0xd6, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_ABSYNC_LISTENER_CONTRACTID			\
  "@mozilla.org/absync/listener;1"

//
// Ab Sync Post Engine
//
#define NS_ABSYNC_POST_ENGINE_CID				  \
{ /* {3C21BB9f-0A87-11d4-8FD6-00A024A7D144} */      \
    0x3c21bb9f, 0xa87, 0x11d4,                      \
  { 0x8f, 0xd6, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_ABSYNC_POST_ENGINE_CONTRACTID			\
  "@mozilla.org/absync/postengine;1"


//
// Ab Sync Post Listener
//
#define NS_ABSYNC_POST_LISTENER_CID				  \
{ /* {3C21BBcc-0A87-11d4-8FD6-00A024A7D144} */      \
    0x3c21bbcc, 0xa87, 0x11d4,                      \
  { 0x8f, 0xd6, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_ABSYNC_POST_LISTENER_CONTRACTID			\
  "@mozilla.org/absync/postlistener;1"
  
//
// Sync Driver
//
#define NS_ADDBOOK_SYNCDRIVER_CONTRACTID \
  "@mozilla.org/addressbook/services/syncdriver;1"
#define NS_ADDBOOK_SYNCDRIVER_CID \
{	/* 40D1D3DA-1637-11d4-8FE1-00A024A7D144 */		    \
  0x40d1d3da, 0x1637, 0x11d4,                       \
  { 0x8f, 0xe1, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } \
}
  
#endif // nsAbSyncCID_h__
