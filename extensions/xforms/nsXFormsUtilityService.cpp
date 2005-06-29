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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsXFormsUtilityService.h"
#include "nsXFormsUtils.h"
#include "nsIXTFElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIXFormsModelElement.h"
#include "nsIDOMNodeList.h"
#include "nsIInstanceElementPrivate.h"
#include "nsIXFormsRepeatElement.h"

NS_IMPL_ISUPPORTS1(nsXFormsUtilityService, nsIXFormsUtilityService)

NS_IMETHODIMP
nsXFormsUtilityService::GetModelFromNode(nsIDOMNode *aNode, 
                                           nsIDOMNode **aModel)
{
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  NS_ASSERTION(aModel, "no return buffer, we'll crash soon");
  *aModel = nsnull;

  nsAutoString namespaceURI;
  aNode->GetNamespaceURI(namespaceURI);

  // If the node is in the XForms namespace and XTF based, then it should
  //   be able to be handled by GetModel.  Otherwise it is probably an instance
  //   node in a instance document.
  if (!namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIModelElementPrivate> modelPriv = nsXFormsUtils::GetModel(element);
  nsCOMPtr<nsIDOMNode> modelElement = do_QueryInterface(modelPriv);
  if( modelElement ) {
    NS_IF_ADDREF(*aModel = modelElement);
  }

  // No model found
  NS_ENSURE_TRUE(*aModel, NS_ERROR_FAILURE);

  return NS_OK;
}


/**
 * Function to see if the given node is associated with the given model.
 * Right now this function is only called by XPath in the case of the
 * instance() function.
 * The provided node can be an instance node from an instance
 * document and thus be associated to the model in that way (model elements
 * contain instance elements).  Otherwise the node will be an XForms element
 * that was used as the context node of the XPath expression (i.e the
 * XForms control has an attribute that contains an XPath expression).
 * Form controls are associated with model elements either explicitly through
 * single-node binding or implicitly (if model cannot by calculated, it
 * will use the first model element encountered in the document).  The model
 * can also be inherited from a containing element like xforms:group or
 * xforms:repeat.
 */
NS_IMETHODIMP
nsXFormsUtilityService::IsNodeAssocWithModel( nsIDOMNode *aNode, 
                                              nsIDOMNode *aModel,
                                              PRBool     *aModelAssocWithNode)
{

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);

  nsAutoString namespaceURI;
  aNode->GetNamespaceURI(namespaceURI);

  // If the node is in the XForms namespace and XTF based, then it should
  //   be able to be handled by GetModel.  Otherwise it is probably an instance
  //   node in a instance document.
  if (namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    nsCOMPtr<nsIModelElementPrivate> modelPriv = nsXFormsUtils::GetModel(element);
    nsCOMPtr<nsIDOMNode> modelNode = do_QueryInterface(modelPriv);

    if (modelNode && (modelNode == aModel)) {
      *aModelAssocWithNode = PR_TRUE;
    } else { 
      *aModelAssocWithNode = PR_FALSE;
    }
  } else { 
    // We are assuming that if the node coming in isn't a proper XForms element,
    //   then it is an instance element in an instance doc.  Now we just have
    //   to determine if the given model contains this instance document.
    nsCOMPtr<nsIDOMDocument> document;
    aNode->GetOwnerDocument(getter_AddRefs(document));
    *aModelAssocWithNode = PR_FALSE;

    // Guess that we'd better make sure that it is a model
    nsCOMPtr<nsIXFormsModelElement> modelEle = do_QueryInterface(aModel);
    if (modelEle) {
      // OK, we know that this is a model element.  So now we have to go
      //   instance element by instance element and find the associated
      //   document.  If it is equal to the document that contains aNode,
      //   then aNode is associated with this aModel element and we can return
      //   true.
      nsCOMPtr<nsIModelElementPrivate>model = do_QueryInterface(modelEle);
      nsCOMPtr<nsIInstanceElementPrivate> instElement;
      nsCOMPtr<nsIDOMDocument> instDocument;

      nsCOMArray<nsIInstanceElementPrivate> *instList = nsnull;
      model->GetInstanceList(&instList);
      NS_ENSURE_TRUE(instList, NS_ERROR_FAILURE);

      for (int i = 0; i < instList->Count(); ++i) {
        instElement = instList->ObjectAt(i);
    
        instElement->GetDocument(getter_AddRefs(instDocument));
        if (instDocument) {
          if (instDocument == document) {
            *aModelAssocWithNode = PR_TRUE;
            break;
          }
        }
      }
    }

  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUtilityService::GetInstanceDocumentRoot(const      nsAString& aID,
                                                  nsIDOMNode *aModelNode,
                                                  nsIDOMNode **aInstanceRoot)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(aInstanceRoot, "no return buffer, we'll crash soon");
  *aInstanceRoot = nsnull;

  if (aID.IsEmpty()) {
    return rv;
  }

  nsCOMPtr<nsIXFormsModelElement> modelElement = do_QueryInterface(aModelNode);
  nsCOMPtr<nsIDOMDocument> doc;
  rv = modelElement->GetInstanceDocument(aID, getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMElement> element;
  rv = doc->GetDocumentElement(getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  if (element) {
    NS_IF_ADDREF(*aInstanceRoot = element);
  }

  return rv;
}

/* Gotta do this via the service since we don't want transformiix to require
 * any of the new extensions, like schema-validation
 */
NS_IMETHODIMP 
nsXFormsUtilityService::ValidateString(const nsAString & aValue, 
                                         const nsAString & aType, 
                                         const nsAString & aNamespace,
                                         PRBool *aResult)
{

  // XXX TODO This function needs to call the XForms validator layer from
  //   bug 274083 when it goes into the build.

#if 0
  nsresult rv = NS_ERROR_FAILURE;
  nsXFormsSchemaValidator *validator = new nsXFormsSchemaValidator();
  *aResult = validator->ValidateString(aValue, aType, aNamespace);
  return rv;
#endif

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXFormsUtilityService::GetRepeatIndex(nsIDOMNode *aRepeat, PRUint32 *aIndex)
{
  NS_ASSERTION(aIndex, "no return buffer for index, we'll crash soon");
  *aIndex = 0;

  nsCOMPtr<nsIXFormsRepeatElement> repeatEle = do_QueryInterface(aRepeat);

  /// 
  /// @bug This should somehow end up in a NaN per the XForms 1.0 Errata (XXX)
  return repeatEle ? repeatEle->GetIndex(aIndex) : NS_OK;
}
