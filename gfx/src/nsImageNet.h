/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "ilINetContext.h"
#include "ilIURL.h"
#include "ilIImageRenderer.h"
#include "ilISystemServices.h"
#include "nscore.h"

class nsIStreamListener;
class nsIURLGroup;
typedef PRBool (*nsReconnectCB)(void* arg, nsIStreamListener* aListener);

extern "C" NS_GFX_(nsresult)
  NS_NewImageNetContext(ilINetContext **aInstancePtrResult,
                        nsIURLGroup* aURLGroup,
                        nsReconnectCB aReconnectCallback,
                        void* aReconnectArg);

extern "C" NS_GFX_(nsresult) 
  NS_NewImageURL(ilIURL **aInstancePtrResult,  
                 const char *aURL, 
                 nsIURLGroup* aURLGroup);

extern "C" NS_GFX_(nsresult) NS_NewImageRenderer(ilIImageRenderer  **aInstancePtrResult);

extern "C" NS_GFX_(nsresult) NS_NewImageSystemServices(ilISystemServices **aInstancePtrResult);

// Internal net context used for synchronously loading icons
nsresult  NS_NewImageNetContextSync(ilINetContext** aInstancePtrResult);
