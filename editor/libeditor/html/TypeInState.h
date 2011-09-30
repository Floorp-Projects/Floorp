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
 * The Original Code is mozilla.org code.
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

#ifndef TypeInState_h__
#define TypeInState_h__

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsISelection.h"
#include "nsISelectionListener.h"
#include "nsEditProperty.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"

struct PropItem
{
  nsIAtom *tag;
  nsString attr;
  nsString value;
  
  PropItem();
  PropItem(nsIAtom *aTag, const nsAString &aAttr, const nsAString &aValue);
  ~PropItem();
};

class TypeInState : public nsISelectionListener
{
public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(TypeInState)

  TypeInState();
  void Reset();
  virtual ~TypeInState();

  nsresult UpdateSelState(nsISelection *aSelection);

  // nsISelectionListener
  NS_DECL_NSISELECTIONLISTENER

  nsresult SetProp(nsIAtom *aProp);
  nsresult SetProp(nsIAtom *aProp, const nsString &aAttr);
  nsresult SetProp(nsIAtom *aProp, const nsString &aAttr, const nsString &aValue);

  nsresult ClearAllProps();
  nsresult ClearProp(nsIAtom *aProp);
  nsresult ClearProp(nsIAtom *aProp, const nsString &aAttr);
  
  //**************************************************************************
  //    TakeClearProperty: hands back next property item on the clear list.
  //                       caller assumes ownership of PropItem and must delete it.
  nsresult TakeClearProperty(PropItem **outPropItem);

  //**************************************************************************
  //    TakeSetProperty: hands back next property item on the set list.
  //                     caller assumes ownership of PropItem and must delete it.
  nsresult TakeSetProperty(PropItem **outPropItem);

  //**************************************************************************
  //    TakeRelativeFontSize: hands back relative font value, which is then
  //                          cleared out.
  nsresult TakeRelativeFontSize(PRInt32 *outRelSize);

  nsresult GetTypingState(bool &isSet, bool &theSetting, nsIAtom *aProp);
  nsresult GetTypingState(bool &isSet, bool &theSetting, nsIAtom *aProp, 
                          const nsString &aAttr);
  nsresult GetTypingState(bool &isSet, bool &theSetting, nsIAtom *aProp, 
                          const nsString &aAttr, nsString* outValue);

  static   bool FindPropInList(nsIAtom *aProp, const nsAString &aAttr, nsAString *outValue, nsTArray<PropItem*> &aList, PRInt32 &outIndex);

protected:

  nsresult RemovePropFromSetList(nsIAtom *aProp, const nsString &aAttr);
  nsresult RemovePropFromClearedList(nsIAtom *aProp, const nsString &aAttr);
  bool IsPropSet(nsIAtom *aProp, const nsString &aAttr, nsString* outValue);
  bool IsPropSet(nsIAtom *aProp, const nsString &aAttr, nsString* outValue, PRInt32 &outIndex);
  bool IsPropCleared(nsIAtom *aProp, const nsString &aAttr);
  bool IsPropCleared(nsIAtom *aProp, const nsString &aAttr, PRInt32 &outIndex);

  nsTArray<PropItem*> mSetArray;
  nsTArray<PropItem*> mClearedArray;
  PRInt32 mRelativeFontSize;
  nsCOMPtr<nsIDOMNode> mLastSelectionContainer;
  PRInt32 mLastSelectionOffset;
  
  friend class nsHTMLEditRules;
};



#endif  // TypeInState_h__

