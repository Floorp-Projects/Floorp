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

/**
 
  Original Author:
  David W. Hyatt(hyatt@netscape.com)

**/

#ifndef nsTreeLayout_h___
#define nsTreeLayout_h___

//#define MOZ_GRID2 1

#ifdef MOZ_GRID2
  #include "nsGridRowGroupLayout.h"
#else
#include "nsTempleLayout.h"
#endif

#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULTreeSliceFrame.h"

class nsIBox;
class nsBoxLayoutState;

#ifdef MOZ_GRID2
class nsTreeLayout : public nsGridRowGroupLayout
#else
class nsTreeLayout : public nsTempleLayout
#endif
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

