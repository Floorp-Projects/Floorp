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
 * Contributor(s): 
 */

#ifndef TypeInState_h__
#define TypeInState_h__

#include "nsISelectionListener.h"
#include "nsIEditProperty.h"
#include "nsString.h"
#include "nsVoidArray.h"

struct PropItem
{
  nsIAtom *tag;
  nsString attr;
  nsString value;
  
  PropItem(nsIAtom *aTag, const nsString &aAttr, const nsString &aValue);
  ~PropItem();
};

class TypeInState : public nsISelectionListener
{
public:

  NS_DECL_ISUPPORTS

  TypeInState();
  void Reset();
  virtual ~TypeInState();

  NS_IMETHOD NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason);

  nsresult SetProp(nsIAtom *aProp);
  nsresult SetProp(nsIAtom *aProp, const nsString &aAttr);
  nsresult SetProp(nsIAtom *aProp, const nsString &aAttr, const nsString &aValue);

  nsresult ClearAllProps();
  nsresult ClearProp(nsIAtom *aProp);
  nsresult ClearProp(nsIAtom *aProp, const nsString &aAttr);
  
  //**************************************************************************
  //    TakeClearProperty: hands back next poroperty item on the clear list.
  //                       caller assumes ownership of PropItem and must delete it.
  nsresult TakeClearProperty(PropItem **outPropItem);

  //**************************************************************************
  //    TakeSetProperty: hands back next poroperty item on the set list.
  //                     caller assumes ownership of PropItem and must delete it.
  nsresult TakeSetProperty(PropItem **outPropItem);

  //**************************************************************************
  //    TakeRelativeFontSize: hands back relative font value, which is then
  //                          cleared out.
  nsresult TakeRelativeFontSize(PRInt32 *outRelSize);

  nsresult GetTypingState(PRBool &isSet, PRBool &theSetting, nsIAtom *aProp);
  nsresult GetTypingState(PRBool &isSet, PRBool &theSetting, nsIAtom *aProp, 
                          const nsString &aAttr);
  nsresult GetTypingState(PRBool &isSet, PRBool &theSetting, nsIAtom *aProp, 
                          const nsString &aAttr, nsString* outValue);

protected:

  nsresult RemovePropFromSetList(nsIAtom *aProp, const nsString &aAttr);
  nsresult RemovePropFromClearedList(nsIAtom *aProp, const nsString &aAttr);
  PRBool IsPropSet(nsIAtom *aProp, const nsString &aAttr, nsString* outValue);
  PRBool IsPropSet(nsIAtom *aProp, const nsString &aAttr, nsString* outValue, PRInt32 &outIndex);
  PRBool IsPropCleared(nsIAtom *aProp, const nsString &aAttr);
  PRBool IsPropCleared(nsIAtom *aProp, const nsString &aAttr, PRInt32 &outIndex);
  PRBool FindPropInList(nsIAtom *aProp, const nsString &aAttr, nsString *outValue, nsVoidArray &aList, PRInt32 &outIndex);

  nsVoidArray mSetArray;
  nsVoidArray mClearedArray;
  PRInt32 mRelativeFontSize;
};



#endif  // TypeInState_h__

