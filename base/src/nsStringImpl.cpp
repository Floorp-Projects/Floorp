/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

  The default implementation for nsIString.

 */


#include "nsIString.h"
#include "nsStr.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIStringIID, NS_ISTRING_IID);

////////////////////////////////////////////////////////////////////////

class nsStringImpl : public nsIString
{
protected:
  nsStr mStr;

public:
  nsStringImpl();
  virtual ~nsStringImpl();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIString interface
  NS_IMETHOD Init(eCharSize aCharSize);
  NS_IMETHOD SetStr(nsStr* aStr);
  NS_IMETHOD GetStr(nsStr* aStr);
  NS_IMETHOD GetImmutableStr(const nsStr** aStr);
};


////////////////////////////////////////////////////////////////////////

nsStringImpl::nsStringImpl()
{
  NS_INIT_REFCNT();
  nsStr::Initialize(mStr, kDefaultCharSize);
}


nsStringImpl::~nsStringImpl()
{
  nsStr::Destroy(mStr);
}

PR_IMPLEMENT(nsresult)
NS_NewString(nsIString** aString)
{
  NS_PRECONDITION(aString != nsnull, "null ptr");
  if (! aString)
    return NS_ERROR_NULL_POINTER;

  nsStringImpl* s = new nsStringImpl();
  if (! s)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(s);
  *aString = s;
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(nsStringImpl);
NS_IMPL_RELEASE(nsStringImpl);
NS_IMPL_QUERY_INTERFACE(nsStringImpl, kIStringIID);

////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStringImpl::Init(eCharSize aCharSize)
{
  nsStr::Initialize(mStr, aCharSize);
  return NS_OK;
}


NS_IMETHODIMP
nsStringImpl::SetStr(nsStr* aStr)
{
  NS_PRECONDITION(aStr != nsnull, "null ptr");
  if (! aStr)
    return NS_ERROR_NULL_POINTER;

  nsStr::Assign(mStr, *aStr, 0, aStr->mLength);
  return NS_OK;
}


NS_IMETHODIMP
nsStringImpl::GetStr(nsStr* aStr)
{
  NS_PRECONDITION(aStr != nsnull, "null ptr");
  if (! aStr)
    return NS_ERROR_NULL_POINTER;

  nsStr::Assign(*aStr, mStr, 0, mStr.mLength);
  return NS_OK;
}



NS_IMETHODIMP
nsStringImpl::GetImmutableStr(const nsStr** aImmutableStr)
{
  NS_PRECONDITION(aImmutableStr != nsnull, "null ptr");
  if (! aImmutableStr)
    return NS_ERROR_NULL_POINTER;

  *aImmutableStr = &mStr;
  return NS_OK;
}
