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
 
  Author:
  Eric D Vaughan

**/

#ifndef nsMonumentLayout_h___
#define nsMonumentLayout_h___

#include "nsSprocketLayout.h"
#include "nsIMonument.h"
class nsTempleLayout;
class nsGridLayout;
class nsBoxLayoutState;
class nsIPresShell;

#define PARENT_STACK_SIZE 100

class nsLayoutIterator
{
public:
  nsLayoutIterator(nsIBox* aBox);
  virtual void Reset();
  virtual PRBool GetNextLayout(nsIBoxLayout** aLayout, PRBool aSearchChildren = PR_FALSE);
  virtual void GetBox(nsIBox** aBox) { *aBox = mBox; }
  virtual PRBool DigDeep(nsIBoxLayout** aLayout, PRBool aSearchChildren);

protected:
  nsIBox* mBox;
  nsIBox* mStartBox;
  PRInt32 mParentCount;
  nsIBox* mParents[PARENT_STACK_SIZE];
};

class nsMonumentIterator: public nsLayoutIterator
{
public:
  nsMonumentIterator(nsIBox* aBox);
  virtual PRBool GetNextMonument(nsIMonument** aMonument, PRBool aSearchChildren = PR_FALSE);
  virtual PRBool GetNextObelisk(nsObeliskLayout** aObelisk, PRBool aSearchChildren = PR_FALSE);
};

// nsBoxSizeListNodeImpl are OWNED by nsBoxSizeListImpl
class nsBoxSizeListNodeImpl : public nsBoxSizeList
{
public:
    virtual nsBoxSize GetBoxSize(nsBoxLayoutState& aState, PRBool aIsHorizontal);
    virtual nsBoxSizeList* GetFirst()          { return nsnull; }
    virtual nsBoxSizeList* GetLast()           { return nsnull; }
    virtual nsBoxSizeList* GetNext()           { return mNext;  }
    virtual nsBoxSizeList* GetParent()         { return mParent;}
    virtual void SetParent(nsBoxSizeList* aParent) { mParent = aParent; }
    virtual PRInt32 GetCount()               { return 1; }
    virtual void SetNext(nsBoxLayoutState& aState, nsBoxSizeList* aNext);
    virtual void Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild);
    virtual void Clear(nsBoxLayoutState& aState) {}
    virtual nsBoxSizeList* GetAt(PRInt32 aIndex);
    virtual nsBoxSizeList* Get(nsIBox* aBox);
    virtual PRBool SetListener(nsIBox* aBox, nsBoxSizeListener& aListener) { return PR_FALSE; }
    virtual void RemoveListener() {}
    virtual void Desecrate(nsBoxLayoutState& aState);
    virtual void MarkDirty(nsBoxLayoutState& aState);
    virtual void AddRef() { mRefCount++; }
    virtual void Release(nsBoxLayoutState& aState);
    virtual void Destroy(nsBoxLayoutState& aState);
    virtual PRInt32 GetRefCount() { return mRefCount; }
    virtual PRBool IsSet() { return mIsSet; }
    virtual nsIBox* GetBox() { return mBox; }

    virtual void SetAdjacent(nsBoxLayoutState& aState, nsBoxSizeList* aAdjacent);
    virtual nsBoxSizeList* GetAdjacent() { return mAdjacent; }
  
    nsBoxSizeListNodeImpl(nsIBox* aBox);
    virtual ~nsBoxSizeListNodeImpl();

    nsBoxSizeList* mNext;
    nsBoxSizeList* mParent; 
    nsBoxSizeList* mAdjacent;  // OWN
    nsIBox* mBox;
    PRInt32 mRefCount;
    PRBool mIsSet;
};

// nsBoxSizeListImpl are OWNED by nsTempleLayout
class nsBoxSizeListImpl : public nsBoxSizeListNodeImpl
{
public:
    virtual nsBoxSize GetBoxSize(nsBoxLayoutState& aState, PRBool aIsHorizontal);
    virtual nsBoxSizeList* GetFirst()        { return mFirst; }
    virtual nsBoxSizeList* GetLast()         { return mLast;  }
    virtual PRInt32 GetCount()               { return mCount; }
    virtual void Desecrate(nsBoxLayoutState& aState);
    virtual void MarkDirty(nsBoxLayoutState& aState);
    virtual void Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild);
    virtual void Clear(nsBoxLayoutState& aState);
    virtual PRBool SetListener(nsIBox* aBox, nsBoxSizeListener& aListener);
    virtual void RemoveListener();
    virtual void Release(nsBoxLayoutState& aState);
    virtual void Destroy(nsBoxLayoutState& aState);

    nsBoxSizeListImpl(nsIBox* aBox);
    virtual ~nsBoxSizeListImpl();

    nsBoxSizeList* mFirst;  // OWN children who are nsBoxSizeListNodeImpl but not nsBoxSizeListImpl
    nsBoxSizeList* mLast;   // OWN children who are nsBoxSizeListNodeImpl but not nsBoxSizeListImpl
    PRInt32 mCount;
    nsBoxSize mBoxSize;
    nsBoxSizeListener* mListener;
    nsIBox* mListenerBox;
};

class nsMonumentLayout : public nsSprocketLayout,
                         public nsIMonument
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD CastToTemple(nsTempleLayout** aTemple);
  NS_IMETHOD CastToObelisk(nsObeliskLayout** aObelisk);
  NS_IMETHOD CastToGrid(nsGridLayout** aGrid);
  NS_IMETHOD GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList);
  NS_IMETHOD GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList);
  NS_IMETHOD BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast, PRBool aIsHorizontal);
  NS_IMETHOD GetParentMonument(nsIBox* aBox, nsCOMPtr<nsIBox>& aParentBox, nsIMonument** aParentMonument);
  NS_IMETHOD GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList);
  NS_IMETHOD EnscriptionChanged(nsBoxLayoutState& aState, PRInt32 aIndex);
  NS_IMETHOD DesecrateMonuments(nsIBox* aBox, nsBoxLayoutState& aState);

protected:
  virtual PRInt32 GetIndexOfChild(nsIBox* aBox, nsIBox* aChild);

  nsMonumentLayout(nsIPresShell* aShell);
};

#endif

