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
 *
 * Contributor(s): 
 */

#ifndef WCI_BROWSERCONTAINER
#define WCI_BROWSERCONTAINER

#include "nsISupports.h"

#include "jni.h"

#define WC_IBROWSERCONTAINER_IID_STR "fdadb2e0-3028-11d4-8a96-0080c7b9c5ba"
#define WC_IBROWSERCONTAINER_IID {0xfdadb2e0, 0x3028, 0x11d4, { 0x8a, 0x96, 0x00, 0x80, 0xc7, 0xb9, 0xc5, 0xba }}

/**

 * This interface defines methods that webclient mozilla must use to
 * support listeners.

 */

class wcIBrowserContainer : public nsISupports {
public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(WC_IBROWSERCONTAINER_IID)


  NS_IMETHOD AddMouseListener(jobject target) = 0;
  NS_IMETHOD AddDocumentLoadListener(jobject target) = 0;
  NS_IMETHOD SetPrompt(jobject target) = 0;
  NS_IMETHOD RemoveAllListeners() = 0;
  NS_IMETHOD RemoveMouseListener() = 0;
  NS_IMETHOD RemoveDocumentLoadListener() = 0;
  NS_IMETHOD GetInstanceCount(PRInt32 *outCount) = 0;

};

#define NS_DECL_WCIBROWSERCONTAINER \
  NS_IMETHOD AddMouseListener(jobject target); \
  NS_IMETHOD AddDocumentLoadListener(jobject target); \
  NS_IMETHOD SetPrompt(jobject target); \
  NS_IMETHOD RemoveAllListeners(); \
  NS_IMETHOD RemoveMouseListener(); \
  NS_IMETHOD RemoveDocumentLoadListener(); \
  NS_IMETHOD GetInstanceCount(PRInt32 *outCount); 


#endif
