/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Simon Fraser <sfraser@netscape.com>
 */



#include "nsIEnumerator.h"

#include "nsCOMPtr.h"
#include "nsVoidArray.h"

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
  
public:

                              nsDocShellEnumerator(PRInt32 inEnumerationDirection);
  virtual                     ~nsDocShellEnumerator();

  // nsISupports
  NS_DECL_ISUPPORTS
  
  // nsISimpleEnumerator
  NS_DECL_NSISIMPLEENUMERATOR
  
public:

  nsresult                    GetEnumerationRootItem(nsIDocShellTreeItem * *aEnumerationRootItem);
  nsresult                    SetEnumerationRootItem(nsIDocShellTreeItem * aEnumerationRootItem);
  
  nsresult                    GetEnumDocShellType(PRInt32 *aEnumerationItemType);
  nsresult                    SetEnumDocShellType(PRInt32 aEnumerationItemType);
    
  nsresult                    First();

protected:

  nsresult                    EnsureDocShellArray();
  nsresult                    ClearState();
  
  nsresult                    BuildDocShellArray(nsVoidArray& inItemArray);
  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsVoidArray& inItemArray) = 0;
    
protected:

  nsIDocShellTreeItem*        mRootItem;      // weak ref!
  
  nsVoidArray*                mItemArray;     // flattened list of items with matching type
  PRInt32                     mCurIndex;
  
  PRInt32                     mDocShellType;  // only want shells of this type

  const PRInt8                mEnumerationDirection;
};


class nsDocShellForwardsEnumerator : public nsDocShellEnumerator
{
public:

                              nsDocShellForwardsEnumerator()
                              : nsDocShellEnumerator(enumerateForwards)
                              {                              
                              }

protected:

  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsVoidArray& inItemArray);

};

class nsDocShellBackwardsEnumerator : public nsDocShellEnumerator
{
public:

                              nsDocShellBackwardsEnumerator()
                              : nsDocShellEnumerator(enumerateBackwards)
                              {                              
                              }
protected:

  virtual nsresult            BuildArrayRecursive(nsIDocShellTreeItem* inItem, nsVoidArray& inItemArray);

};
