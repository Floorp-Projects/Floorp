/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTransformMediator.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIServiceManagerUtils.h"
#include "nsObserverService.h"
#include "nsString.h"

nsresult
NS_NewTransformMediator(nsITransformMediator** aInstancePtrResult,                     
                        const nsACString& aMimeType)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsTransformMediator* it;
  NS_NEWXPCOM(it, nsTransformMediator);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aMimeType);
  if (NS_FAILED(rv)) {
    delete it;
    return rv;
  }
  return CallQueryInterface(it, aInstancePtrResult);
}

nsTransformMediator::nsTransformMediator() : mEnabled(PR_FALSE),
                                             mStyleInvalid(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsTransformMediator::~nsTransformMediator()
{
}

static
nsresult ConstructContractID(nsACString& aContractID,
                             const nsACString& aMimeType)
{
  aContractID.Assign(NS_LITERAL_CSTRING("@mozilla.org/document-transformer;1?type="));
  aContractID.Append(aMimeType);
  return NS_OK;
}

nsresult
nsTransformMediator::Init(const nsACString& aMimeType)
{
  // Construct prog ID for the document tranformer component
  nsCAutoString contractID;  
  nsresult rv = ConstructContractID(contractID, aMimeType);
  if (NS_SUCCEEDED(rv)) {
    // Try to find a component that implements the nsIDocumentTransformer interface
    mTransformer = do_CreateInstance(contractID.get(), &rv);
  }
  return rv;
}

// nsISupports
NS_IMPL_ISUPPORTS1(nsTransformMediator, nsITransformMediator)

void
nsTransformMediator::TryToTransform()
{
  if (mSourceDOM && mStyleDOM && mResultDoc && mObserver) 
  {
    if (mEnabled && mTransformer) {
      mTransformer->TransformDocument(mSourceDOM, 
                                      mStyleDOM,
                                      mResultDoc,
                                      mObserver);
    }
    else if (mStyleInvalid) {
      // Copy the error message from the stylesheet document to the result
      // result document and notify the observer.
      nsCOMPtr<nsIDOMElement> docElement;
      mResultDoc->GetDocumentElement(getter_AddRefs(docElement));
      nsCOMPtr<nsIDOMNode> newRoot, root;
      mResultDoc->ImportNode(mStyleDOM, PR_TRUE, getter_AddRefs(newRoot));
      if (docElement) {
        nsCOMPtr<nsIDOMNode> origRoot;
        root = newRoot;
        mResultDoc->ReplaceChild(docElement, root, getter_AddRefs(origRoot));
      }
      else {
        mResultDoc->AppendChild(newRoot, getter_AddRefs(root));
      }

      nsresult rv;
      nsCOMPtr<nsIObserverService> anObserverService =
          do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        anObserverService->AddObserver(mObserver, "xslt-done", PR_TRUE);
        anObserverService->NotifyObservers(root, "xslt-done", nsnull);
      }
    }
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

NS_IMETHODIMP
nsTransformMediator::SetStyleInvalid(PRBool aInvalid)
{
  mStyleInvalid = aInvalid;
  return NS_OK;
}
