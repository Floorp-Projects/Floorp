/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsSchemaPrivate.h"

////////////////////////////////////////////////////////////
//
// nsSchemaComponentBase implementation
//
////////////////////////////////////////////////////////////
nsSchemaComponentBase::nsSchemaComponentBase(nsSchema* aSchema) 
  : mSchema(aSchema), mIsResolved(PR_FALSE), mIsCleared(PR_FALSE)
{
}

nsSchemaComponentBase::~nsSchemaComponentBase()
{
  mSchema = nsnull;
}

/* readonly attribute wstring targetNamespace; */
NS_IMETHODIMP 
nsSchemaComponentBase::GetTargetNamespace(nsAString& aTargetNamespace)
{
  if (mSchema) {
    return mSchema->GetTargetNamespace(aTargetNamespace);
  }

  aTargetNamespace.Truncate();
  return NS_OK;
}
