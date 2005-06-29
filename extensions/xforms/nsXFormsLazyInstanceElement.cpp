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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Reed <aaronr@us.ibm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXFormsLazyInstanceElement.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIDOMDOMImplementation.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsLazyInstanceElement,
                             nsXFormsStubElement,
                             nsIInstanceElementPrivate,
                             nsIInterfaceRequestor)

nsXFormsLazyInstanceElement::nsXFormsLazyInstanceElement()
{
}

// nsIInstanceElementPrivate

NS_IMETHODIMP
nsXFormsLazyInstanceElement::GetDocument(nsIDOMDocument **aDocument)
{
  NS_IF_ADDREF(*aDocument = mDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsLazyInstanceElement::SetDocument(nsIDOMDocument *aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}


NS_IMETHODIMP
nsXFormsLazyInstanceElement::BackupOriginalDocument()
{
  nsresult rv = NS_OK;

  // This is called when xforms-ready is received by the model.  By now we know 
  // that the lazy instance document has been populated and is loaded into 
  // mDocument.  Get the root node, clone it, and insert it into our copy of 
  // the document.  This is the magic behind getting xforms-reset to work.
  if(mDocument && mOriginalDocument) {
    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMElement> instanceRoot;
    rv = mDocument->GetDocumentElement(getter_AddRefs(instanceRoot));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(instanceRoot, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> nodeReturn;
    rv = instanceRoot->CloneNode(PR_TRUE, getter_AddRefs(newNode)); 
    if(NS_SUCCEEDED(rv)) {
      rv = mOriginalDocument->AppendChild(newNode, getter_AddRefs(nodeReturn));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), 
                       "failed to set up original instance document");
    }
  }
  return rv;
}

NS_IMETHODIMP
nsXFormsLazyInstanceElement::RestoreOriginalDocument()
{
  nsresult rv = NS_OK;

  // This is called when xforms-reset is received by the model.  We assume
  // that the backup of the lazy instance document has been populated and is
  // loaded into mOriginalDocument.  Get the backup's root node, clone it, and 
  // insert it into the live copy of the instance document.  This is the magic 
  // behind getting xforms-reset to work. 
  if(mDocument && mOriginalDocument) {
    nsCOMPtr<nsIDOMNode> newNode, instanceRootNode, nodeReturn;
    nsCOMPtr<nsIDOMElement> instanceRoot;

    // first remove all the old stuff
    rv = mDocument->GetDocumentElement(getter_AddRefs(instanceRoot));
    if(NS_SUCCEEDED(rv)) {
      if(instanceRoot) {
        rv = mDocument->RemoveChild(instanceRoot, getter_AddRefs(nodeReturn));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // now all of the garbage is out o' there!  Put the original data back
    // into mDocument
    rv = mOriginalDocument->GetDocumentElement(getter_AddRefs(instanceRoot));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(instanceRoot, NS_ERROR_FAILURE);
    instanceRootNode = do_QueryInterface(instanceRoot);

    rv = instanceRootNode->CloneNode(PR_TRUE, getter_AddRefs(newNode)); 
    if(NS_SUCCEEDED(rv)) {
      rv = mDocument->AppendChild(newNode, getter_AddRefs(nodeReturn));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), 
                       "failed to restore original instance document");
    }
  }
  return rv;
}

NS_IMETHODIMP
nsXFormsLazyInstanceElement::GetElement(nsIDOMElement **aElement)
{
  *aElement = nsnull;
  return NS_OK;  
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsXFormsLazyInstanceElement::GetInterface(const nsIID & aIID, void **aResult)
{
  *aResult = nsnull;
  return QueryInterface(aIID, aResult);
}

nsresult
nsXFormsLazyInstanceElement::CreateLazyInstanceDocument(nsIDOMDocument *aXFormsDocument)
{
  NS_ENSURE_ARG(aXFormsDocument);

  // Do not try to load an instance document if the current document is not
  // associated with a DOM window.  This could happen, for example, if some
  // XForms document loaded itself as instance data (which is what the Forms
  // 1.0 testsuite does).
  nsCOMPtr<nsIDocument> d = do_QueryInterface(aXFormsDocument);
  if (d && !d->GetScriptGlobalObject())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDOMImplementation> domImpl;
  nsresult rv = aXFormsDocument->GetImplementation(getter_AddRefs(domImpl));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = domImpl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                                 getter_AddRefs(mDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Lazy authored instance documents have a root named "instanceData"
  nsCOMPtr<nsIDOMElement> instanceDataElement;
  nsCOMPtr<nsIDOMNode> childReturn;
  rv = mDocument->CreateElementNS(EmptyString(), 
                                  NS_LITERAL_STRING("instanceData"),
                                  getter_AddRefs(instanceDataElement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDocument->AppendChild(instanceDataElement, getter_AddRefs(childReturn));
  NS_ENSURE_SUCCESS(rv, rv);

  // I don't know if not being able to create a backup document is worth
  // failing this function.  Since it probably won't be used often, we'll
  // let it slide.  But it probably does mean that things are going south
  // with the browser.
  domImpl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                          getter_AddRefs(mOriginalDocument));
  return rv;
}
