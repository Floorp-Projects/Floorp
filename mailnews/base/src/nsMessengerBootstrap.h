/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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


#ifndef __nsMessenger_h
#define __nsMessenger_h

#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsICmdLineHandler.h"
#include "nsIMessengerWindowService.h"

#define NS_MESSENGERBOOTSTRAP_CID                 \
{ /* 4a85a5d0-cddd-11d2-b7f6-00805f05ffa5 */      \
  0x4a85a5d0, 0xcddd, 0x11d2,                     \
  {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}

#define NS_MESSENGEROPTIONSSTARTUP_CID            \
{ /* 87A35D6C-9BB9-49f8-9CAC-5D5214550B2D */      \
  0x87a35d6c, 0x9bb9, 0x49f8,                     \
  { 0x9c, 0xac, 0x5d, 0x52, 0x14, 0x55, 0xb, 0x2d }}

class nsMessengerBootstrap : public nsICmdLineHandler, public nsIMessengerWindowService {
  
public:
  nsMessengerBootstrap();
  virtual ~nsMessengerBootstrap();
  
  NS_DECL_ISUPPORTS  
  NS_DECL_NSICMDLINEHANDLER
  NS_DECL_NSIMESSENGERWINDOWSERVICE
  CMDLINEHANDLER_REGISTERPROC_DECLS
  
};

#ifdef MOZ_THUNDERBIRD

// thunderbird has a new command line argument called -options which launches
// the Options dialog.

class nsMsgOptionsCmdLineHandler : public nsICmdLineHandler
{
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMDLINEHANDLER
  
  nsMsgOptionsCmdLineHandler();
  virtual ~nsMsgOptionsCmdLineHandler();

  CMDLINEHANDLER_REGISTERPROC_DECLS
};
 
#endif

#endif
