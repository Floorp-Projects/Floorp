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

#include "ilINetContext.h"
#include "ilIURL.h"
#include "ilIImageRenderer.h"
#include "ilISystemServices.h"
#include "nscore.h"

class nsIStreamListener;
class nsILoadGroup;

typedef PRBool (*nsReconnectCB)(void* arg, nsIStreamListener* aListener);

extern "C" NS_GFX_(nsresult)
  NS_NewImageNetContext(ilINetContext **aInstancePtrResult,
                        nsILoadGroup* aLoadGroup,
                        nsReconnectCB aReconnectCallback,
                        void* aReconnectArg);

extern "C" NS_GFX_(nsresult) 
  NS_NewImageURL(ilIURL **aInstancePtrResult,  
                 const char *aURL , nsILoadGroup* aLoadGroup);

extern "C" NS_GFX_(nsresult) NS_NewImageRenderer(ilIImageRenderer  **aInstancePtrResult);

extern "C" NS_GFX_(nsresult) NS_NewImageSystemServices(ilISystemServices **aInstancePtrResult);

// Internal net context used for synchronously loading icons
nsresult  NS_NewImageNetContextSync(ilINetContext** aInstancePtrResult);
