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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsITreeFrame_h__
#define nsITreeFrame_h__

#include "nsISupports.h"

#define BLAH_IID_STR "06b8921c-1dd2-11b2-96f2-d5925c3d48ca"

#define NS_ITREEFRAME_IID \
  {0x06b8921c, 0x1dd2, 0x11b2, \
    { 0x96, 0xf2, 0xd5, 0x92, 0x5c, 0x3d, 0x48, 0xca }}


class nsITreeFrame : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITREEFRAME_IID)

  NS_IMETHOD EnsureRowIsVisible(PRInt32 aRowIndex) = 0;
  NS_IMETHOD GetNextItem(nsIDOMElement* aStartItem, PRInt32 aDelta, nsIDOMElement** aResult) = 0;
  NS_IMETHOD GetPreviousItem(nsIDOMElement* aStartItem, PRInt32 aDelta, nsIDOMElement** aResult) = 0;
  NS_IMETHOD ScrollToIndex(PRInt32 aRowIndex) = 0;
  NS_IMETHOD GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** aResult) = 0;
  NS_IMETHOD GetIndexOfItem(nsIPresContext* aPresContext, nsIDOMElement* aElement, PRInt32* aResult) = 0;
  NS_IMETHOD GetNumberOfVisibleRows(PRInt32* aResult) = 0;
  NS_IMETHOD GetIndexOfFirstVisibleRow(PRInt32* aResult) = 0;
  NS_IMETHOD GetRowCount(PRInt32* aResult) = 0;
  NS_IMETHOD BeginBatch()=0;
  NS_IMETHOD EndBatch()=0;
};

#endif
