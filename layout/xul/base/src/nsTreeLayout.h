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

/**
 
  Original Author:
  David W. Hyatt(hyatt@netscape.com)

**/

#ifndef nsTreeLayout_h___
#define nsTreeLayout_h___

#include "nsTempleLayout.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULTreeSliceFrame.h"

class nsIBox;
class nsBoxLayoutState;

class nsTreeLayout : public nsTempleLayout
{
public:
  nsTreeLayout(nsIPresShell* aShell);

  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  
  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  NS_IMETHOD LazyRowCreator(nsBoxLayoutState& aState, nsXULTreeGroupFrame* aGroup);

protected:
  nsXULTreeOuterGroupFrame* GetOuterFrame(nsIBox* aBox);
  nsXULTreeGroupFrame* GetGroupFrame(nsIBox* aBox);
  nsXULTreeSliceFrame* GetRowFrame(nsIBox* aBox);

private:
  NS_IMETHOD LayoutInternal(nsIBox* aBox, nsBoxLayoutState& aState);
};

#endif

