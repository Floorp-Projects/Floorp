/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCOMPtr.h"
#include "nsTransformMediator.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kITransformMediatorIID, NS_ITRANSFORM_MEDIATOR_IID);

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
  return it->QueryInterface(kITransformMediatorIID, (void **)aResult);
}

nsTransformMediator::nsTransformMediator()
{
  NS_INIT_REFCNT();
  mEnabled = PR_FALSE;
  mTransformer = nsnull;
  mSourceDOM = nsnull;
  mStyleDOM = nsnull;
  mCurrentDoc = nsnull;
  mObserver = nsnull;
}

nsTransformMediator::~nsTransformMediator()
{
  NS_IF_RELEASE(mTransformer);
  NS_IF_RELEASE(mSourceDOM);
  NS_IF_RELEASE(mStyleDOM);
  mCurrentDoc = nsnull;
  NS_IF_RELEASE(mObserver);
}

static
nsresult ConstructProgID(nsString& aProgID, const nsString& aMimeType)
{
  aProgID = kTransformerProgIDPrefix;
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
    rv = nsComponentManager::ProgIDToCLSID((const char*)progIDStr, &cid);
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
NS_IMPL_ISUPPORTS(nsTransformMediator, kITransformMediatorIID)


void
nsTransformMediator::TryToTransform()
{
  if (mEnabled && mSourceDOM && 
      mStyleDOM && mCurrentDoc && 
      mObserver && mTransformer) 
  {
    mTransformer->TransformDocument(mSourceDOM, 
                                         mStyleDOM,
                                         mCurrentDoc,
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
nsTransformMediator::SetSourceContentModel(nsIDOMElement* aSource)
{
  NS_IF_RELEASE(mSourceDOM);
  mSourceDOM = aSource;
  NS_IF_ADDREF(mSourceDOM);
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetStyleSheetContentModel(nsIDOMElement* aStyle)
{
  NS_IF_RELEASE(mStyleDOM);
  mStyleDOM = aStyle;
  NS_IF_ADDREF(mStyleDOM);
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetCurrentDocument(nsIDOMDocument* aDoc)
{
  mCurrentDoc = aDoc;
  TryToTransform();
  return NS_OK;
}

NS_IMETHODIMP
nsTransformMediator::SetTransformObserver(nsIObserver* aObserver)
{
  NS_IF_RELEASE(mObserver);
  mObserver = aObserver;
  NS_IF_ADDREF(mObserver);
  TryToTransform();
  return NS_OK;
}

