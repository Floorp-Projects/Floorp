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
 *   Darin Fisher <darin@meer.net>
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

#include "nsXFormsStubElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIXFormsControl.h"
#include "nsISchema.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsXFormsAtoms.h"
#include "nsAutoPtr.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMFocusListener.h"
#include "nsXFormsUtils.h"
#include "nsIModelElementPrivate.h"
#include "nsIContent.h"
#include "nsIDOMXPathExpression.h"
#include "nsNetUtil.h"


static void
ReleaseObject(void    *aObject,
              nsIAtom *aPropertyName,
              void    *aPropertyValue,
              void    *aData)
{
  NS_STATIC_CAST(nsISupports *, aObject)->Release();
}


/**
 * Implementation of the \<upload\> element.
 */
class nsXFormsUploadElement : public nsXFormsXMLVisualStub,
                              public nsIDOMFocusListener,
                              public nsIXFormsControl
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aPoint);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD DocumentChanged(nsIDOMDocument *aNewDocument);
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent *aEvent);
  NS_IMETHOD Blur(nsIDOMEvent *aEvent);

  nsXFormsUploadElement() : mElement(nsnull) {}

private:
  nsCOMPtr<nsIDOMElement>           mLabel;
  nsCOMPtr<nsIDOMHTMLInputElement>  mInput;
  nsIDOMElement                    *mElement;
};

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsUploadElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIDOMFocusListener,
                             nsIDOMEventListener)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsUploadElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_DOCUMENT_CHANGED |
                                nsIXTFElement::NOTIFY_PARENT_CHANGED);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // Our anonymous content structure will look like this:
  //
  // <label>                         (mLabel)
  //   <span/>                       (insertion point)
  //   <input/>                      (mInput)
  // </label>

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("label"),
                          getter_AddRefs(mLabel));
  NS_ENSURE_STATE(mLabel);

  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDOMNode> childReturn;

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(element));
  NS_ENSURE_STATE(element);

  mLabel->AppendChild(element, getter_AddRefs(childReturn));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("input"),
                          getter_AddRefs(element));
  mInput = do_QueryInterface(element);
  NS_ENSURE_STATE(mInput);

  mInput->SetType(NS_LITERAL_STRING("file"));

  mLabel->AppendChild(mInput, getter_AddRefs(childReturn));

  // We can't use xtf handleDefault here because editor stops blur events from
  // bubbling, and we can't use a system event group handler because blur
  // events don't go to the system event group (bug 263240).  So, just install
  // a bubbling listener in the normal event group.  This works out because
  // content can't get at the anonymous content to install an event handler,
  // and the event doesn't bubble up.

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mInput);
  NS_ASSERTION(targ, "input must be an event target!");

  targ->AddEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsUploadElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mLabel);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::GetInsertionPoint(nsIDOMElement **aPoint)
{
  nsCOMPtr<nsIDOMNode> childNode;
  mLabel->GetFirstChild(getter_AddRefs(childNode));
  return CallQueryInterface(childNode, aPoint);
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsUploadElement::OnDestroyed()
{
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mInput);
  if (NS_LIKELY(targ != nsnull)) {
    targ->RemoveEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  // We need to re-evaluate our instance data binding when our document
  // changes, since our context can change
  if (aNewDocument)
    Refresh();  

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::ParentChanged(nsIDOMElement *aNewParent)
{
  // We need to re-evaluate our instance data binding when our parent changes,
  // since xmlns declarations or our context could have changed.
  if (aNewParent)
    Refresh();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIDOMNode> modelNode = nsXFormsUtils::GetModel(mElement);

    nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);    
    if (model)
      model->RemoveFormControl(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    Refresh();
  }

  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsUploadElement::HandleEvent(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::Focus(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::Blur(nsIDOMEvent *aEvent)
{
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                       NS_LITERAL_STRING("ref"),
                                       EmptyString(),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));

  if (!result)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> singleNode;
  result->GetSingleNodeValue(getter_AddRefs(singleNode));

  if (!singleNode)
    return NS_OK;

  nsAutoString value;
  mInput->GetValue(value);

  // store the file as a property on the selected content node.  the submission
  // code will read this value.

  nsCOMPtr<nsIContent> content = do_QueryInterface(singleNode);
  NS_ENSURE_STATE(content);

  nsILocalFile *file = nsnull;
  NS_NewLocalFile(value, PR_FALSE, &file);
  NS_ENSURE_STATE(file);

  content->SetProperty(nsXFormsAtoms::uploadFileProperty, file, ReleaseObject);

  // update the instance data node.  this value is never used by the submission
  // code.  instead, it exists so that the instance data will appear to be in-
  // sync with what is actually submitted.
  nsCAutoString spec;
  NS_GetURLSpecFromFile(file, spec);
  nsXFormsUtils::SetNodeValue(singleNode, NS_ConvertUTF8toUTF16(spec));
  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsUploadElement::Refresh()
{
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                       NS_LITERAL_STRING("ref"),
                                       EmptyString(),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));

  nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);

  // @bug / todo: If \<input\> has binding attributes that are invalid, we
  // should clear the content. But the content should be left if the element
  // is unbound.
  // @see https://bugzilla.mozilla.org/show_bug.cgi?id=265216
  if (!model)
    return NS_OK;

  model->AddFormControl(this);

  nsCOMPtr<nsIDOMNode> resultNode;
  if (result)
    result->GetSingleNodeValue(getter_AddRefs(resultNode));

  if (!resultNode)
    return NS_OK;

  // find out if the control should be made readonly
  PRBool isReadOnly = PR_FALSE;
  nsCOMPtr<nsIContent> nodeContent = do_QueryInterface(resultNode);
  if (nodeContent) {
    nsIDOMXPathExpression *expr =
      NS_STATIC_CAST(nsIDOMXPathExpression*,
                     nodeContent->GetProperty(nsXFormsAtoms::readonly));

    if (expr) {
      expr->Evaluate(mElement,
                     nsIDOMXPathResult::BOOLEAN_TYPE, nsnull,
                     getter_AddRefs(result));
      if (result) {
        result->GetBooleanValue(&isReadOnly);
      }
    }
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(resultNode);
  NS_ENSURE_STATE(content);

  nsILocalFile *file =
      NS_STATIC_CAST(nsILocalFile *,
                     content->GetProperty(nsXFormsAtoms::uploadFileProperty));

  // get the text value for the control
  nsAutoString value;
  if (file)
    file->GetPath(value);

  mInput->SetValue(value);
  mInput->SetReadOnly(isReadOnly);

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsUploadElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsUploadElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
