/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * nsRange.cpp: Implementation of the nsIDOMRange object.
 */

#include "nsIDOMRange.h"

static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);

class nsRange : public nsIDOMRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);
  NS_IMETHOD    SetIsPositioned(PRBool aIsPositioned);

  NS_IMETHOD    GetStartParent(nsIDOMElement** aStartParent);
  NS_IMETHOD    SetStartParent(nsIDOMElement* aStartParent);

  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);
  NS_IMETHOD    SetStartOffset(PRInt32 aStartOffset);

  NS_IMETHOD    GetEndParent(nsIDOMElement** aEndParent);
  NS_IMETHOD    SetEndParent(nsIDOMElement* aEndParent);

  NS_IMETHOD    GetEndOffset(PRInt32* aEndOffset);
  NS_IMETHOD    SetEndOffset(PRInt32 aEndOffset);

  NS_IMETHOD    GetIsCollapsed(PRBool* aIsCollapsed);
  NS_IMETHOD    SetIsCollapsed(PRBool aIsCollapsed);

  NS_IMETHOD    GetCommonParent(nsIDOMNode** aCommonParent);
  NS_IMETHOD    SetCommonParent(nsIDOMNode* aCommonParent);

  NS_IMETHOD    SetStart(nsIDOMNode* aParent, PRInt32 aOffset);

  NS_IMETHOD    SetEnd(nsIDOMNode* aParent, PRInt32 aOffset);

  NS_IMETHOD    Collapse(PRBool aToStart);

  NS_IMETHOD    Unposition();

  NS_IMETHOD    SelectNode(nsIDOMNode* aN);

  NS_IMETHOD    SelectNodeContents(nsIDOMNode* aN);

  NS_IMETHOD    DeleteContents();

  NS_IMETHOD    ExtractContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    CopyContents(nsIDOMDocumentFragment** aReturn);

  NS_IMETHOD    InsertNode(nsIDOMNode* aN);

  NS_IMETHOD    SurroundContents(nsIDOMNode* aN);

  NS_IMETHOD    Clone(nsIDOMRange** aReturn);

  NS_IMETHOD    ToString(nsString& aReturn);
};

nsresult
NS_NewRange(nsIDOMRange** aInstancePtrResult)
{
  nsRange * range = new nsRange();
  return range->QueryInterface(kIRangeIID, (void**) aInstancePtrResult);
}

nsresult nsRange::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRangeIID)) {
    nsIDOMRange* tmp = this;
    *aInstancePtrResult = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}

nsRange::nsRange() {
  NS_INIT_REFCNT();
} 

nsRange::~nsRange() {
} 

NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)

nsresult nsRange::GetIsPositioned(PRBool* aIsPositioned)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetIsPositioned(PRBool aIsPositioned)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetStartParent(nsIDOMElement** aStartParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetStartParent(nsIDOMElement* aStartParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetStartOffset(PRInt32 aStartOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetEndParent(nsIDOMElement** aEndParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetEndParent(nsIDOMElement* aEndParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetEndOffset(PRInt32 aEndOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetIsCollapsed(PRBool* aIsCollapsed)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetIsCollapsed(PRBool aIsCollapsed)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::GetCommonParent(nsIDOMNode** aCommonParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetCommonParent(nsIDOMNode* aCommonParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::Collapse(PRBool aToStart)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::Unposition()
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::DeleteContents()
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::CopyContents(nsIDOMDocumentFragment** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::Clone(nsIDOMRange** aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::ToString(nsString& aReturn)
{ return NS_ERROR_NOT_IMPLEMENTED; }

