/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#ifndef __nsMessenger_h
#define __nsMessenger_h

#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsIAppShellComponent.h"
#include "nsICmdLineHandler.h"
#include "nsIMessengerWindowService.h"

#define NS_MESSENGERBOOTSTRAP_CID                 \
{ /* 4a85a5d0-cddd-11d2-b7f6-00805f05ffa5 */      \
  0x4a85a5d0, 0xcddd, 0x11d2,                     \
  {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}


class nsMessengerBootstrap : public nsIAppShellComponent, public nsICmdLineHandler, public nsIMessengerWindowService {
  
public:
  nsMessengerBootstrap();
  virtual ~nsMessengerBootstrap();
  
  NS_DECL_ISUPPORTS  
  NS_DECL_NSIAPPSHELLCOMPONENT
  NS_DECL_NSICMDLINEHANDLER
  NS_DECL_NSIMESSENGERWINDOWSERVICE
  CMDLINEHANDLER_REGISTERPROC_DECLS
  
};


#endif
