/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellEnumerator_h___
#define nsDocShellEnumerator_h___

#include "nsISimpleEnumerator.h"
#include "nsTArray.h"
#include "nsIWeakReferenceUtils.h"

class nsIDocShellTreeItem;

/*
// {13cbc281-35ae-11d5-be5b-bde0edece43c}
#define NS_DOCSHELL_FORWARDS_ENUMERATOR_CID  \
{ 0x13cbc281, 0x35ae, 0x11d5, { 0xbe, 0x5b, 0xbd, 0xe0, 0xed, 0xec, 0xe4, 0x3c } }

#define NS_DOCSHELL_FORWARDS_ENUMERATOR_CONTRACTID \
"@mozilla.org/docshell/enumerator-forwards;1"

// {13cbc282-35ae-11d5-be5b-bde0edece43c}
#define NS_DOCSHELL_BACKWARDS_ENUMERATOR_CID  \
{ 0x13cbc282, 0x35ae, 0x11d5, { 0xbe, 0x5b, 0xbd, 0xe0, 0xed, 0xec, 0xe4, 0x3c } }

#define NS_DOCSHELL_BACKWARDS_ENUMERATOR_CONTRACTID \
"@mozilla.org/docshell/enumerator-backwards;1"
*/

class nsDocShellEnumerator : public nsISimpleEnumerator
{
protected:

  enum {
    enumerateForwards,
    enumerateBackwards
  };

  virtual                     ~nsDocShellEnumerator();

public:

                              explicit nsDocShellEnumerator(int32_t inEnumerationDirection);

  // nsISupports
  NS_DECL_ISUPPORTS
  
  // nsISimpleEnumerator
  NS_DECL_NSISIMPLEENUMERATOR
  
public:

  nsresult                    GetEnumerationRootItem(nsIDocShellTreeItem * *aEnumerationRootItem);
  nsresult                    SetEnumerationRootItem(nsIDocShellTreeItem * aEnumerationRootItem);
  
  nsresult                    GetEnumDocShellType(int32_t *aEnumerationItemType);
  nsresult                    SetEnumDocShellType(int32_t aEnumerationItemType);
    
  nsresult                    First();

protected:

  nsresult                    EnsureDocShellArray();
  nsresult                    ClearState();
  
  nsresult                    BuildDocShellArray(nsTArray<nsWeakPtr>& inItemArray);
  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsTArray<nsWeakPtr>& inItemArray) = 0;
    
protected:

  nsWeakPtr                   mRootItem;      // weak ref!
  
  nsTArray<nsWeakPtr>         mItemArray;     // flattened list of items with matching type
  uint32_t                    mCurIndex;
  
  int32_t                     mDocShellType;  // only want shells of this type
  bool                        mArrayValid;    // is mItemArray up to date?

  const int8_t                mEnumerationDirection;
};


class nsDocShellForwardsEnumerator : public nsDocShellEnumerator
{
public:

                              nsDocShellForwardsEnumerator()
                              : nsDocShellEnumerator(enumerateForwards)
                              {                              
                              }

protected:

  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsTArray<nsWeakPtr>& inItemArray);

};

class nsDocShellBackwardsEnumerator : public nsDocShellEnumerator
{
public:

                              nsDocShellBackwardsEnumerator()
                              : nsDocShellEnumerator(enumerateBackwards)
                              {                              
                              }
protected:

  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsTArray<nsWeakPtr>& inItemArray);
};

#endif // nsDocShellEnumerator_h___
