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
#include "nsCOMPtr.h"
#include "nsTransformMediator.h"
#include "nsIComponentManager.h"

const char* kTransformerProgIDPrefix = "component://netscape/document-transformer?type=";

nsresult
NS_NewTransformMediator(nsITransformMediator** aResult,                     
                     const nsString& aMimeType)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTransformMediator* it;
  NS_NEWXPCOM(it, nsTransformMediator);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aMimeType);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(NS_GET_IID(nsITransformMediator), (void **)aResult);
}

nsTransformMediator::nsTransformMediator()
{
  NS_INIT_REFCNT();
  mEnabled = PR_FALSE;
}

nsTransformMediator::~nsTransformMediator()
{
}

static
nsresult ConstructProgID(nsString& aProgID, const nsString& aMimeType)
{
  aProgID.AssignWithConversion(kTransformerProgIDPrefix);
  aProgID.Append(aMimeType);

  return NS_OK;
}

nsresult
nsTransformMediator::Init(const nsString& aMimeType)
{
  nsString progID;  
  nsresult rv = NS_OK;

  // Construct prog ID for the document tranformer component
  rv = ConstructProgID(progID, aMimeType);
  if (NS_SUCCEEDED(rv)) {
    nsCID cid;
    char* progIDStr = (char*)progID.ToNewCString();
    rv = nsComponentManager::ProgIDToClassID((const char*)progIDStr, &cid);
    if (NS_SUCCEEDED(rv)) {
      // Try to find a component that implements the nsIDocumentTransformer interface
      rv = nsComponentManager::CreateInstance(cid, nsnull,
        NS_GET_IID(nsIDocumentTransformer), (void**) &mTransformer);
    }
    delete [] progIDStr;
  }

  return rv;
}

// nsISupports
NS_IMPL_ISUPPORTS(nsTransformMediator, NS_GET_IID(nsITransformMediator))


void
nsTransformMediator::TryToTransform()
{
  if (mEnabled && mSourceDOM && 
      mStyleDOM && mResultDoc && 
      mObserver && mTransformer) 
  {
    mTransformer->TransformDocument(mSourceDOM, 
                                         mStyleDOM,
                                         mResultDoc,
                                         mObserver);
  }
}

// nsITransformMediator
NS_IMETHODIMP
nsTransformMediator::SetEnabled(PRBool aValue)
{
  mEnabled = aValue;
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetSourceContentModel(nsIDOMNode* aSource)
{
  mSourceDOM = aSource;
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetStyleSheetContentModel(nsIDOMNode* aStyle)
{
  mStyleDOM = aStyle;
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetResultDocument(nsIDOMDocument* aDoc)
{
  mResultDoc = aDoc;
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::GetResultDocument(nsIDOMDocument** aDoc)
{
  *aDoc = mResultDoc;
  NS_IF_ADDREF(*aDoc);
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetTransformObserver(nsIObserver* aObserver)
{
  mObserver = aObserver;
  TryToTransform();
  return NS_OK;
}

