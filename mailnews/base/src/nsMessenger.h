/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#ifndef __nsMessenger_h
#define __nsMessenger_h

#include "nscore.h"
#include "nsIMessenger.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"

#define NS_MESSENGER_CID                          \
{ /* 241471d0-cdda-11d2-b7f6-00805f05ffa5 */      \
  0x241471d0, 0xcdda, 0x11d2,                     \
  {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}

#define NS_MESSENGERBOOTSTRAP_CID                 \
{ /* 4a85a5d0-cddd-11d2-b7f6-00805f05ffa5 */      \
  0x4a85a5d0, 0xcddd, 0x11d2,                     \
  {0xb7, 0xf6, 0x00, 0x80, 0x5f, 0x05, 0xff, 0xa5}}


NS_BEGIN_EXTERN_C

nsresult NS_NewMessenger(const nsIID &aIID, void **inst);

nsresult NS_NewMessengerBootstrap(const nsIID &aIID, void ** inst);

NS_END_EXTERN_C

#endif
