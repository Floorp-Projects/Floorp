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

#ifndef nsDOM_h__
#define nsDOM_h__

#include "nsISupports.h"

class nsIDOMDocument;
class nsString;

#define NS_IDOM_IID \
{ /* 8f6bca70-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca70, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * The "DOM" interface provides a number of methods for performing operations that are 
 * independent of any particular instance of the document object model. 
 */
class nsIDOM : public nsISupports {
public:
  /**
   * Creates a document of the specified type 
   *
   * @param type [in]       The type of document to create, i.e., "HTML" or "XML"
   * @param aDocument [out] An instance of an HTML or XML document is created. Documents are a
   *                        special type of DOM object because other objects (such as Elements,
   *                        DocumentFragments, etc.) can only exist within the context of a Document
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD CreateDocument(nsString &type, nsIDOMDocument** aDocument) = 0;

  /**
   * Returns TRUE if the current DOM implements a given feature, FALSE otherwise 
   *
   * @param aFeature [in]   The package name of the feature to test
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD HasFeature(nsString &aFeature) = 0;
};


#endif // nsDOM_h__
