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
 * The Original Code is Mozilla Communicator client code.
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

#ifndef nsITreeFrame_h__
#define nsITreeFrame_h__

#include "nsISupports.h"
class nsIDOMElement;

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
