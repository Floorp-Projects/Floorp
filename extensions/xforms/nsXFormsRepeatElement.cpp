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
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#include "nsIXTFXMLVisualWrapper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsISchema.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsSubstring.h"

#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMXPathResult.h"

#include "nsXFormsControlStub.h"
#include "nsIXFormsContextControl.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsModelElement.h"
#include "nsXFormsUtils.h"

#ifdef DEBUG
//#define DEBUG_XF_REPEAT
#endif

/**
 * Implementation of the XForms \<repeat\> control.
 * @see http://www.w3.org/TR/xforms/slice9.html#id2632123
 *
 * On Refresh(), nsXFormsRepeatElement, does the following for each node in
 * the nodeset the \<repeat\> tag is bound to:
 *
 * 1) Creates a new \<contextcontainer\> (nsXFormsContextContainer)
 *
 * 2) Clones all its children (that is children of its nsIXTFXMLVisualWrapper)
 *    and appends them as children to the nsXFormsContextContainer
 * 
 * 3) Sets the context node and size for the nsXFormsContextContainer, so
 *    that children can retrieve this through nsIXFormsContextControl.
 *
 * 4) Inserts the nsXFormsContextContainer into its visual content node
 *    (mHTMLElement).
 *
 * For example, this instance data:
 * <pre>
 * <instance>
 *   <data>
 *     <n>val1</n>
 *     <n>val2</n>
 *   </data>
 * </instance>
 * </pre>
 *
 * and this repeat:
 * <pre>
 * <repeat nodeset="n">
 *   Val: <output ref="."/>
 * </repeat>
 * </pre>
 *
 * will be expanded to:
 * <pre>
 * <repeat nodeset="n">
 *   <contextcontainer>         (contextNode == "n[0]" and contextPosition == 1)
 *     Val: <output ref="."/>   (that is: 'val1')
 *   </contextcontainer>
 *   <contextcontainer>         (contextNode == "n[1]" and contextPosition == 2)
 *     Val: <output ref="."/>   (that is: 'val2')
 *   </contextcontainer>
 * </repeat>
 * </pre>
 *
 * Besides being a practical way to implement \<repeat\>, it also means that it
 * is possible to CSS-style the individual "rows" in a \<repeat\>.
 *
 * @todo Support attribute based repeats (XXX), as in:
 *       \<html:table xforms:repeat-nodeset="..."\>
 *       @see http://www.w3.org/TR/xforms/index-all.html#ui.repeat.via.attrs
 */
class nsXFormsRepeatElement : public nsXFormsControlStub
{
protected:
  /** The HTML representation for the node */
  nsCOMPtr<nsIDOMHTMLDivElement> mHTMLElement;

  /** True while children are being added */
  PRBool mAddingChildren;
  
   /**
   * Retrieves an integer attribute and checks its type.
   * 
   * @param aName            The attribute to retrieve
   * @param aVal             The value
   * @param aType            The attribute (Schema) type
   * @return                 Normal error codes, and NS_ERROR_NOT_AVAILABLE if
   *                         the attribute was empty/nonexistant
   */
  nsresult GetIntAttr(const nsAString &aName,
                      PRInt32         *aVal,
                      const PRUint16   aType);

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);
  
  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsControl
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);

  nsXFormsRepeatElement() : mAddingChildren(PR_FALSE) {};  
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsRepeatElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl)


// nsIXTFXMLVisual
NS_IMETHODIMP
nsXFormsRepeatElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::OnCreated(aWrapper=%p)\n", (void*) aWrapper);
#endif

  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Create UI element
  nsCOMPtr<nsIDOMElement> domElement;
  rv = domDoc->CreateElementNS(NS_LITERAL_STRING("http://www.w3.org/1999/xhtml"),
                               NS_LITERAL_STRING("div"),
                               getter_AddRefs(domElement));
  NS_ENSURE_SUCCESS(rv, rv);
  mHTMLElement = do_QueryInterface(domElement);
  NS_ENSURE_TRUE(mHTMLElement, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::GetVisualContent(nsIDOMElement **aElement)
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::GetVisualContent()\n");
#endif
  
  NS_IF_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

// nsIXTFElement
NS_IMETHODIMP
nsXFormsRepeatElement::OnDestroyed()
{
  mHTMLElement = nsnull;

  return nsXFormsControlStub::OnDestroyed();
}

NS_IMETHODIMP
nsXFormsRepeatElement::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::WillSetAttribute(aName=%p, aValue='%s')\n", (void*) aName, NS_ConvertUCS2toUTF8(aValue).get());
#endif
  
  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::nodeset ||
      aName == nsXFormsAtoms::model) {
    nsCOMPtr<nsIModelElementPrivate> model = nsXFormsUtils::GetModel(mElement);
    if (model) {
      model->RemoveFormControl(this);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::AttributeSet(aName=%p, aValue=%s)\n", (void*) aName, NS_ConvertUCS2toUTF8(aValue).get());
#endif

  if (aName == nsXFormsAtoms::bind ||
      aName == nsXFormsAtoms::nodeset ||
      aName == nsXFormsAtoms::model) {
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::BeginAddingChildren()
{
  mAddingChildren = PR_TRUE;
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsRepeatElement::DoneAddingChildren()
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::DoneAddingChildren()\n");
#endif
  
  mAddingChildren = PR_FALSE;

  Refresh();
  
  return NS_OK;
}


// nsXFormsControl

NS_IMETHODIMP
nsXFormsRepeatElement::Refresh()
{
#ifdef DEBUG_XF_REPEAT
  printf("nsXFormsRepeatElement::Refresh()\n");
#endif

  if (!mHTMLElement || mAddingChildren)
    return NS_OK;

  nsresult rv;

  // Clear any existing children
  nsCOMPtr<nsIDOMNode> cNode;
  mHTMLElement->GetFirstChild(getter_AddRefs(cNode));
  while (cNode) {
    nsCOMPtr<nsIDOMNode> retNode;
    mHTMLElement->RemoveChild(cNode, getter_AddRefs(retNode));
    mHTMLElement->GetFirstChild(getter_AddRefs(cNode));
  }

  // Get the nodeset we are bound to
  nsCOMPtr<nsIDOMXPathResult> result;
  nsCOMPtr<nsIModelElementPrivate> model;
  rv = ProcessNodeBinding(NS_LITERAL_STRING("nodeset"),
                          nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                          getter_AddRefs(result),
                          getter_AddRefs(model));

  if (NS_FAILED(rv) | !result) {
    return rv;
  }
                                         
  /// @todo The result should be a _homogenous_ collection (spec. 9.3.1),
  /// do/can/should we check this? (XXX)
  PRUint32 contextSize;
  rv = result->GetSnapshotLength(&contextSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (model && contextSize > 0) {
    // Get model ID
    nsCOMPtr<nsIDOMElement> modelElement = do_QueryInterface(model);
    NS_ENSURE_TRUE(modelElement, NS_ERROR_FAILURE);
    nsAutoString modelID;
    modelElement->GetAttribute(NS_LITERAL_STRING("id"), modelID);

    // Get attributes
    PRUint32 startIndex;
    rv = GetIntAttr(NS_LITERAL_STRING("startIndex"),
                    (PRInt32*) &startIndex,
                    nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        startIndex = 1;
      } else {
        return rv;
      }
    }
      
    PRUint32 number;
    rv = GetIntAttr(NS_LITERAL_STRING("number"),
                    (PRInt32*) &number,
                    nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_NOT_AVAILABLE) {
        number = contextSize;
      } else {
        return rv;
      }
    }
    // Get DOM document
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mHTMLElement->GetOwnerDocument(getter_AddRefs(domDoc));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString strSize;
    strSize.AppendInt(contextSize);

    // The spec. states the following for startIndex: "Optional 1-based
    // initial value of the repeat index", I interpret this as "start
    // showing element at position |startIndex|". So I iterate over the
    // interval [max(1, startIndex),min(contextSize, number + startIndex)[
    for (PRUint32 i = NS_MAX((PRUint32) 1, startIndex);
         i < NS_MIN(contextSize + 1, number + startIndex);
         ++i) {
      // Create <contextcontainer>
      nsCOMPtr<nsIDOMElement> riElement;
      rv = domDoc->CreateElementNS(NS_LITERAL_STRING("http://www.w3.org/2002/xforms"),
                                   NS_LITERAL_STRING("contextcontainer"),
                                   getter_AddRefs(riElement));
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Set context size, context position, and model as attributes
      if (!modelID.IsEmpty()) {
        riElement->SetAttribute(NS_LITERAL_STRING("model"), modelID);
      }  
      nsAutoString strPos; strPos.AppendInt(i);
      riElement->SetAttribute(NS_LITERAL_STRING("contextposition"), strPos);
      riElement->SetAttribute(NS_LITERAL_STRING("contextsize"), strSize);

      // Get and set context node
      nsCOMPtr<nsIXFormsContextControl> riContext = do_QueryInterface(riElement);
      NS_ENSURE_TRUE(riContext, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMNode> contextNode;
      rv = result->SnapshotItem(i - 1, getter_AddRefs(contextNode));
      NS_ENSURE_SUCCESS(rv, rv);
        
      nsCOMPtr<nsIDOMElement> contextElement = do_QueryInterface(contextNode);
      NS_ENSURE_TRUE(contextElement, NS_ERROR_FAILURE);
        
      rv = riContext->SetContextNode(contextElement);
      NS_ENSURE_SUCCESS(rv, rv);

      // Iterate over template children, clone them, and append them to <contextcontainer>
      nsCOMPtr<nsIDOMNode> child;
      rv = mElement->GetFirstChild(getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);
      while (child) {
        nsCOMPtr<nsIDOMNode> childClone;
        rv = child->CloneNode(PR_TRUE, getter_AddRefs(childClone));
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCOMPtr<nsIDOMNode> newNode;
        rv = riElement->AppendChild(childClone, getter_AddRefs(newNode));
        NS_ENSURE_SUCCESS(rv, rv);
        
        rv = child->GetNextSibling(getter_AddRefs(newNode));
        NS_ENSURE_SUCCESS(rv, rv);
        child = newNode;
      }

      // Append node
      nsCOMPtr<nsIDOMNode> domNode;
      rv = mHTMLElement->AppendChild(riElement, getter_AddRefs(domNode));
      NS_ENSURE_SUCCESS(rv, rv);

      // There is an awfull lot of evaluating being done by all the
      // children, as they are created and inserted into the different
      // places in the DOM, the only refresh necessary is the one when they
      // are appended in mHTMLElement.
    }
  }
 
  return NS_OK;
}

/**
 * @todo "Setting focus to a repeating structure sets the focus to
 *        the repeat item represented by the repeat index."
 */
NS_IMETHODIMP
nsXFormsRepeatElement::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}

/**
 * @todo This function will be part of the general schema support, so it will
 * only live here until this is implemented there. (XXX)
 */
nsresult
nsXFormsRepeatElement::GetIntAttr(const nsAString& aName, PRInt32* aVal, const PRUint16 aType)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(aVal);

  nsAutoString attrVal;
  mElement->GetAttribute(aName, attrVal);

  /// @todo Is this the correct error to return? We need to distinguish between
  /// an empty attribute and other errors. (XXX)
  if (attrVal.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  PRInt32 errCode;
  /// @todo ToInteger is extremely large, "xxx23xxx" will be parsed with no errors
  /// as "23"... (XXX)
  *aVal = attrVal.ToInteger(&errCode);
  NS_ENSURE_TRUE(errCode == 0, NS_ERROR_FAILURE);

  ///
  /// @todo Check maximum values? (XXX)
  switch (aType) {
  case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
    if (*aVal < 0) {
     rv = NS_ERROR_FAILURE;
    }
    break;
  
  case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
    if (*aVal <= 0) {
      rv = NS_ERROR_FAILURE;
    }
    break;

  case nsISchemaBuiltinType::BUILTIN_TYPE_ANYTYPE:
    break;

  default:
    rv = NS_ERROR_INVALID_ARG; // or NOT_IMPLEMENTED?
    break;
  }
  
  return rv;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsRepeatElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsRepeatElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
