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
#include "nsIDOMNode.h"

static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);

class nsRange : public nsIDOMRange
{
public:
  NS_DECL_ISUPPORTS

  nsRange();
  virtual ~nsRange();

  NS_IMETHOD    GetIsPositioned(PRBool* aIsPositioned);
  NS_IMETHOD    SetIsPositioned(PRBool aIsPositioned);

  NS_IMETHOD    GetStartParent(nsIDOMNode** aStartParent);
  NS_IMETHOD    SetStartParent(nsIDOMNode* aStartParent);

  NS_IMETHOD    GetStartOffset(PRInt32* aStartOffset);
  NS_IMETHOD    SetStartOffset(PRInt32 aStartOffset);

  NS_IMETHOD    GetEndParent(nsIDOMNode** aEndParent);
  NS_IMETHOD    SetEndParent(nsIDOMNode* aEndParent);

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

private:
  PRBool mIsPositioned;
  nsIDOMNode* mStartParent;
  PRInt32 mStartOffset;
  nsIDOMNode* mEndParent;
  PRInt32 mEndOffset;

  PRBool IsIncreasing(nsIDOMNode* sParent, PRInt32 sOffset,
                       nsIDOMNode* eParent, PRInt32 eOffset);
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

  mIsPositioned = PR_FALSE;
  nsIDOMNode* mStartParent = NULL;
  PRInt32 mStartOffset = 0;
  nsIDOMNode* mEndParent = NULL;
  PRInt32 mEndOffset = 0;
} 

nsRange::~nsRange() {
} 

NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)

PRBool nsRange::IsIncreasing(nsIDOMNode* sParent, PRInt32 sOffset,
                              nsIDOMNode* eParent, PRInt32 eOffset)
{
  // XXX NEED IMPLEMENTATION!
  return PR_TRUE;
}

nsresult nsRange::GetIsPositioned(PRBool* aIsPositioned)
{
  *aIsPositioned = mIsPositioned;
  return NS_OK;
}

nsresult nsRange::GetStartParent(nsIDOMNode** aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartParent)
    return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(*aStartParent);
  NS_IF_ADDREF(mStartParent);
  *aStartParent = mStartParent;
  return NS_OK;
}

nsresult nsRange::SetStartParent(nsIDOMNode* aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(aStartParent, mStartOffset,
                    mEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_RELEASE(mStartParent);
  NS_IF_ADDREF(aStartParent);
  mStartParent = aStartParent;
  return NS_OK;
}

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartOffset)
    return NS_ERROR_NULL_POINTER;
  *aStartOffset = mStartOffset;
  return NS_OK;
}

nsresult nsRange::SetStartOffset(PRInt32 aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, aStartOffset,
                    mEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  mStartOffset = aStartOffset;
  return NS_OK;
}

nsresult nsRange::GetEndParent(nsIDOMNode** aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndParent)
    return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(*aEndParent);
  NS_IF_ADDREF(mEndParent);
  *aEndParent = mEndParent;
  return NS_OK;
}

nsresult nsRange::SetEndParent(nsIDOMNode* aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, mStartOffset,
                    aEndParent, mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  NS_IF_RELEASE(mEndParent);
  NS_IF_ADDREF(aEndParent);
  mEndParent = aEndParent;
  return NS_OK;
}

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndOffset)
    return NS_ERROR_NULL_POINTER;
  *aEndOffset = mEndOffset;
  return NS_OK;
}

nsresult nsRange::SetEndOffset(PRInt32 aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!IsIncreasing(mStartParent, mStartOffset,
                    mEndParent, aEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;

  mEndOffset = aEndOffset;
  return NS_OK;
}

nsresult nsRange::GetIsCollapsed(PRBool* aIsCollapsed)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (mEndParent == 0 ||
      (mStartParent == mEndParent && mStartOffset == mEndOffset))
    *aIsCollapsed = PR_TRUE;
  else
    *aIsCollapsed = PR_FALSE;
  return NS_OK;
}

nsresult nsRange::GetCommonParent(nsIDOMNode** aCommonParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if (!mIsPositioned)
  {
    NS_IF_RELEASE(mEndParent);
    mEndParent = NULL;
    mEndOffset = NULL;
    mIsPositioned = PR_TRUE;
  }
  NS_IF_ADDREF(aParent);
  mStartParent = aParent;
  mStartOffset = aOffset;
  return NS_OK;
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;  // can't set end before start

  NS_IF_ADDREF(aParent);
  mEndParent = aParent;
  mEndOffset = aOffset;
  return NS_OK;
}

nsresult nsRange::Collapse(PRBool aToStart)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  if (aToStart)
  {
    NS_IF_RELEASE(mEndParent);
    NS_IF_ADDREF(mStartParent);
    mEndParent = mStartParent;
    mEndOffset = mStartOffset;
    return NS_OK;
  }
  else
  {
    NS_IF_RELEASE(mStartParent);
    NS_IF_ADDREF(mEndParent);
    mStartParent = mEndParent;
    mStartOffset = mEndOffset;
    return NS_OK;
  }
}

nsresult nsRange::Unposition()
{
  NS_IF_RELEASE(mStartParent);
  mStartParent = NULL;
  mStartOffset = 0;
  NS_IF_RELEASE(mEndParent);
  mEndParent = NULL;
  mEndOffset = 0;
  mIsPositioned = PR_FALSE;
  return NS_OK;
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  nsIDOMNode * parent;
  nsresult res = aN->GetParentNode(&parent);
  if (!NS_SUCCEEDED(res))
    return res;

  if (mIsPositioned)
    Unposition();
  NS_IF_ADDREF(parent);
  mStartParent = parent;
  mStartOffset = 0;      // XXX NO DIRECT WAY TO GET CHILD # OF THIS NODE!
  NS_IF_ADDREF(parent);
  mEndParent = parent;
  mEndOffset = mStartOffset;
  return NS_OK;
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  if (mIsPositioned)
    Unposition();

  NS_IF_ADDREF(aN);
  mStartParent = aN;
  mStartOffset = 0;
  NS_IF_ADDREF(aN);
  mEndParent = aN;
  mEndOffset = 0;        // WRONG!  SHOULD BE # OF LAST CHILD!
  return NS_OK;
}

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

//
// We don't actually want to allow setting this ...
// it should be set only by actually positioning the range.
//
nsresult nsRange::SetIsPositioned(PRBool aIsPositioned)
{ return NS_ERROR_NOT_IMPLEMENTED; }

//
// Various other things which we don't want to implement yet:
//
nsresult nsRange::SetIsCollapsed(PRBool aIsCollapsed)
{ return NS_ERROR_NOT_IMPLEMENTED; }

nsresult nsRange::SetCommonParent(nsIDOMNode* aCommonParent)
{ return NS_ERROR_NOT_IMPLEMENTED; }

