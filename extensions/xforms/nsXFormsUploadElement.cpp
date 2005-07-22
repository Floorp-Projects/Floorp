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

#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDOMDocument.h"
#include "nsXFormsControlStub.h"
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
  NS_STATIC_CAST(nsISupports *, aPropertyValue)->Release();
}


/**
 * Implementation of the \<upload\> element.
 */
class nsXFormsUploadElement : public nsIDOMFocusListener,
                              public nsXFormsControlStub
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
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);

  // nsIXFormsControl
  NS_IMETHOD Refresh();
  NS_IMETHOD TryFocus(PRBool* aOK);

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent *aEvent);
  NS_IMETHOD Blur(nsIDOMEvent *aEvent);

private:
  nsCOMPtr<nsIDOMElement>           mLabel;
  nsCOMPtr<nsIDOMHTMLInputElement>  mInput;
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsUploadElement,
                             nsXFormsControlStub,
                             nsIDOMFocusListener,
                             nsIDOMEventListener)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsUploadElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  // Our anonymous content structure will look like this:
  //
  // <label>                         (mLabel)
  //   <span/>                       (insertion point)
  //   <input/>                      (mInput)
  // </label>

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

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

NS_IMETHODIMP
nsXFormsUploadElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::accesskey) {
    // accesskey
    mInput->SetAttribute(NS_LITERAL_STRING("accesskey"), aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::AttributeRemoved(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::accesskey) {
    // accesskey
    mInput->RemoveAttribute(NS_LITERAL_STRING("accesskey"));
  }

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

  return nsXFormsControlStub::OnDestroyed();
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
  if (!mInput || !mBoundNode || !mModel ||
      !nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;

  nsAutoString value;
  mInput->GetValue(value);

  // store the file as a property on the selected content node.  the submission
  // code will read this value.

  nsCOMPtr<nsIContent> content = do_QueryInterface(mBoundNode);
  NS_ENSURE_STATE(content);

  nsILocalFile *file = nsnull;
  nsCAutoString spec;

  // We need to special case an empty value.  If the input field is blank,
  // then I assume that the intention is that all bound nodes will become
  // (or stay) empty and that there shouldn't be any file kept in the property.
  if (!value.IsEmpty()) {
    NS_NewLocalFile(value, PR_FALSE, &file);
    NS_ENSURE_STATE(file);
    NS_GetURLSpecFromFile(file, spec);
    content->SetProperty(nsXFormsAtoms::uploadFileProperty, file, 
                         ReleaseObject);
  } else {
    content->DeleteProperty(nsXFormsAtoms::uploadFileProperty);
  }


  // update the instance data node.  this value is never used by the submission
  // code.  instead, it exists so that the instance data will appear to be in-
  // sync with what is actually submitted.

  PRBool changed;
  nsresult rv = mModel->SetNodeValue(mBoundNode, NS_ConvertUTF8toUTF16(spec), 
                                     &changed);
  NS_ENSURE_SUCCESS(rv, rv);
  if (changed) {
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(mModel);
    if (model) {
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Recalculate);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Revalidate);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = nsXFormsUtils::DispatchEvent(model, eEvent_Refresh);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsUploadElement::Refresh()
{
  if (!mInput || !mBoundNode)
    return NS_OK;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mBoundNode);
  NS_ENSURE_STATE(content);

  nsILocalFile *file =
      NS_STATIC_CAST(nsILocalFile *,
                     content->GetProperty(nsXFormsAtoms::uploadFileProperty));

  // get the text value for the control
  nsAutoString value;
  if (file)
    file->GetPath(value);

  mInput->SetValue(value);
  mInput->SetReadOnly(GetReadOnlyState());

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUploadElement::TryFocus(PRBool* aOK)
{
  *aOK = GetRelevantState() && nsXFormsUtils::FocusControl(mInput);
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
