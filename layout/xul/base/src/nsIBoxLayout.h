/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsIBoxLayout_h___
#define nsIBoxLayout_h___

#include "nsISupports.h"
#include "nsIFrame.h"

class nsPresContext;
class nsBoxLayout;
class nsBoxLayoutState;
class nsIRenderingContext;
struct nsRect;

// c9bf9fe7-a2f4-4f38-bbed-11a05633d676
#define NS_IBOX_LAYOUT_IID \
{ 0xc9bf9fe7, 0xa2f4, 0x4f38, \
  { 0xbb, 0xed 0x11, 0xa0, 0x56, 0x33, 0xd6, 0x76 } }

class nsIBoxLayout : public nsISupports {

public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBOX_LAYOUT_IID)

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState)=0;

  virtual nsSize GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)=0;
  virtual nsSize GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)=0;
  virtual nsSize GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)=0;
  virtual nscoord GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)=0;

  virtual void ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList)=0;
  virtual void ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)=0;
  virtual void ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)=0;
  virtual void ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)=0;
  virtual void IntrinsicWidthsDirty(nsIBox* aBox, nsBoxLayoutState& aState)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBoxLayout, NS_IBOX_LAYOUT_IID)

#endif
