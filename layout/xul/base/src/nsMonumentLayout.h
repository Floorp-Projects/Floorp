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
 
  Author:
  Eric D Vaughan

**/

#ifndef nsMonumentLayout_h___
#define nsMonumentLayout_h___

#include "nsSprocketLayout.h"
#include "nsIMonument.h"
class nsTempleLayout;
class nsBoxLayoutState;
class nsIPresShell;

class nsBoxSizeListNodeImpl : public nsBoxSizeList
{
public:
    virtual nsBoxSize GetBoxSize(nsBoxLayoutState& aState)   { return mBoxSize; }
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

    virtual void Desecrate();
    virtual void AddRef() { mRefCount++; }
    virtual void Release(nsBoxLayoutState& aState);
    virtual PRInt32 GetRefCount() { return mRefCount; }
    virtual PRBool IsSet() { return mIsSet; }
    virtual nsIBox* GetBox() { return mBox; }


    nsBoxSizeListNodeImpl(nsIBox* aBox);

    nsBoxSizeList* mNext;
    nsBoxSizeList* mParent;
    nsBoxSize mBoxSize;
    nsIBox* mBox;
    PRInt32 mRefCount;
    PRBool mIsSet;
};

class nsBoxSizeListImpl : public nsBoxSizeListNodeImpl
{
public:
    virtual nsBoxSize GetBoxSize(nsBoxLayoutState& aState);
    virtual nsBoxSizeList* GetFirst()        { return mFirst; }
    virtual nsBoxSizeList* GetLast()         { return mLast;  }
    virtual PRInt32 GetCount()               { return mCount; }
    virtual void Desecrate();
    virtual void Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild);
    virtual void Clear(nsBoxLayoutState& aState);

    nsBoxSizeListImpl(nsIBox* aBox);

    nsBoxSizeList* mFirst;
    nsBoxSizeList* mLast;
    PRInt32 mCount;
};

class nsMonumentLayout : public nsSprocketLayout,
                         public nsIMonument
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CastToTemple(nsTempleLayout** aTemple);
  NS_IMETHOD CastToObelisk(nsObeliskLayout** aObelisk);
  NS_IMETHOD GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList);
  NS_IMETHOD GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList);
  //NS_IMETHOD CountMonuments(PRInt32& aCount);
  NS_IMETHOD BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize** aFirst, nsBoxSize** aLast);
  NS_IMETHOD GetParentMonument(nsIBox* aBox, nsIMonument** aMonument);
  NS_IMETHOD GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList);

  //virtual void SetHorizontal(PRBool aIsHorizontal);
protected:
  //virtual PRBool GetInitialOrientation(PRBool& aIsHorizontal); 
  nsMonumentLayout(nsIPresShell* aShell);
};

#endif

