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


#ifndef __nsComposer_h
#define __nsComposer_h

#include "nscore.h"
#include "nsIComposer.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"

#define NS_COMPOSER_CID                          \
{ /* 82041530-D73E-11d2-82A9-00805F2A0107 */      \
  0x82041530, 0xd73e, 0x11d2,                     \
  {0x82, 0xa9, 0x0, 0x80, 0x5f, 0x2a, 0x1, 0x7}}

#define NS_COMPOSERBOOTSTRAP_CID                 \
{ /* 82041531-D73E-11d2-82A9-00805F2A0107 */      \
  0x82041531, 0xd73e, 0x11d2,                     \
  {0x82, 0xa9, 0x0, 0x80, 0x5f, 0x2a, 0x1, 0x7}}


NS_BEGIN_EXTERN_C

nsresult
NS_NewComposer(const nsIID &aIID, void **inst);

nsresult NS_NewComposerBootstrap(const nsIID &aIID, void ** inst, nsIServiceManager* serviceManager);

NS_END_EXTERN_C

#endif /*__nsComposer_h*/
