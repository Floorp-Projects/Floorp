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
 * Author:
 * Eric D Vaughan
 *
 * Contributor(s): 
 */

#ifndef nsIMonument_h___
#define nsIMonument_h___

#include "nsISupports.h"
#include "nsIFrame.h"

class nsIBox;
class nsBoxLayoutState;
class nsTempleLayout;
class nsObeliskLayout;
class nsMonumentLayout;
class nsBoxLayoutState;
class nsIPresShell;

class nsBoxSizeList
{
public:
    virtual nsBoxSize GetBoxSize(nsBoxLayoutState& aState)=0;
    virtual nsBoxSizeList* GetFirst()=0;
    virtual nsBoxSizeList* GetLast()=0;
    virtual nsBoxSizeList* GetNext()=0;
    virtual nsBoxSizeList* GetParent()=0;
    virtual nsBoxSizeList* GetAt(PRInt32 aIndex)=0;
    virtual void SetParent(nsBoxSizeList* aParent)=0;
    virtual void SetNext(nsBoxLayoutState& aState, nsBoxSizeList* aNext)=0;
    virtual void Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild)=0;
    virtual void Clear(nsBoxLayoutState& aState)=0;
    virtual PRInt32 GetCount()=0;
    virtual void Desecrate()=0;
    virtual void AddRef()=0;
    virtual void Release(nsBoxLayoutState& aState)=0;
    virtual PRBool IsSet()=0;
    virtual nsIBox* GetBox()=0;
    virtual PRInt32 GetRefCount()=0;
};

// {AF0C1603-06C3-11d4-BA07-001083023C1E}
#define NS_IMONUMENT_IID { 0xaf0c1603, 0x6c3, 0x11d4, { 0xba, 0x7, 0x0, 0x10, 0x83, 0x2, 0x3c, 0x1e } };

class nsIMonument : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IBOX_LAYOUT_IID; return iid; }

  NS_IMETHOD CastToTemple(nsTempleLayout** aTemple)=0;
  NS_IMETHOD CastToObelisk(nsObeliskLayout** aObelisk)=0;
  NS_IMETHOD GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList)=0;
  NS_IMETHOD GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor = nsnull)=0;
  NS_IMETHOD GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor = nsnull)=0;
  NS_IMETHOD GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList)=0;
  //NS_IMETHOD CountMonuments(PRInt32& aCount)=0;
  NS_IMETHOD BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize** aFirst, nsBoxSize** aLast)=0;
  NS_IMETHOD GetParentMonument(nsIBox* aBox, nsIMonument** aMonument)=0;
  NS_IMETHOD GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList)=0;

};

#endif

