/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMText.h"
#include "nsIDOMNode.h"
#include "nsGenericElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"

using namespace mozilla::dom;

/**
 * Implementation of &lt;option&gt;
 */

nsGenericHTMLElement*
NS_NewHTMLOptionElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                        FromParser aFromParser)
{
  /*
   * nsHTMLOptionElement's will be created without a nsINodeInfo passed in
   * if someone says "var opt = new Option();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nsnull);

    nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::option, nsnull,
                                                   kNameSpaceID_XHTML);
    NS_ENSURE_TRUE(nodeInfo, nsnull);
  }

  return new nsHTMLOptionElement(nodeInfo.forget());
}

nsHTMLOptionElement::nsHTMLOptionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mSelectedChanged(PR_FALSE),
    mIsSelected(PR_FALSE),
    mIsInSetDefaultSelected(PR_FALSE)
{
}

nsHTMLOptionElement::~nsHTMLOptionElement()
{
}

// ISupports


NS_IMPL_ADDREF_INHERITED(nsHTMLOptionElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLOptionElement, nsHTMLOptionElement)

// QueryInterface implementation for nsHTMLOptionElement
NS_INTERFACE_TABLE_HEAD(nsHTMLOptionElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLOptionElement,
                                   nsIDOMHTMLOptionElement,
                                   nsIJSNativeInitializer)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLOptionElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLOptionElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLOptionElement)


NS_IMETHODIMP
nsHTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  nsCOMPtr<nsIDOMHTMLSelectElement> selectControl =
    do_QueryInterface(GetSelect());

  if (selectControl) {
    selectControl->GetForm(aForm);
  }

  return NS_OK;
}

void
nsHTMLOptionElement::SetSelectedInternal(PRBool aValue, PRBool aNotify)
{
  mSelectedChanged = PR_TRUE;
  mIsSelected = aValue;

  // When mIsInSetDefaultSelected is true, the notification will be handled by
  // SetAttr/UnsetAttr.
  if (aNotify && !mIsInSetDefaultSelected) {
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      mozAutoDocUpdate upd(document, UPDATE_CONTENT_STATE, aNotify);
      document->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_CHECKED);
    }
  }
}

NS_IMETHODIMP
nsHTMLOptionElement::SetValue(const nsAString& aValue)
{
  SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetValue(nsAString& aValue)
{
  // If the value attr is there, that is *exactly* what we use.  If it is
  // not, we compress whitespace .text.
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue)) {
    GetText(aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionElement::GetSelected(PRBool* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = PR_FALSE;

  // If we haven't been explictly selected or deselected, use our default value
  if (!mSelectedChanged) {
    return GetDefaultSelected(aValue);
  }

  *aValue = mIsSelected;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetSelected(PRBool aValue)
{
  // Note: The select content obj maintains all the PresState
  // so defer to it to get the answer
  nsCOMPtr<nsISelectElement> selectInt = do_QueryInterface(GetSelect());
  if (selectInt) {
    PRInt32 index;
    GetIndex(&index);
    // This should end up calling SetSelectedInternal
    return selectInt->SetOptionsSelectedByIndex(index, index, aValue,
                                                PR_FALSE, PR_TRUE, PR_TRUE,
                                                nsnull);
  } else {
    SetSelectedInternal(aValue, PR_TRUE);
    return NS_OK;
  }

  return NS_OK;
}

NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, DefaultSelected, selected)
NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Label, label)
//NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Value, value)
NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, Disabled, disabled)

NS_IMETHODIMP 
nsHTMLOptionElement::GetIndex(PRInt32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = -1; // -1 indicates the index was not found

  // Get our containing select content object.
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement =
    do_QueryInterface(GetSelect());

  if (selectElement) {
    // Get the options from the select object.
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
    selectElement->GetOptions(getter_AddRefs(options));

    if (options) {
      // Walk the options to find out where we are in the list (ick, O(n))
      PRUint32 length = 0;
      options->GetLength(&length);

      nsCOMPtr<nsIDOMNode> thisOption;

      for (PRUint32 i = 0; i < length; i++) {
        options->Item(i, getter_AddRefs(thisOption));

        if (thisOption.get() == static_cast<nsIDOMNode *>(this)) {
          *aIndex = i;

          break;
        }
      }
    }
  }

  return NS_OK;
}

nsChangeHint
nsHTMLOptionElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);

  if (aAttribute == nsGkAtoms::label ||
      aAttribute == nsGkAtoms::text) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

nsresult
nsHTMLOptionElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                   const nsAString* aValue, PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::BeforeSetAttr(aNamespaceID, aName,
                                                    aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNamespaceID != kNameSpaceID_None || aName != nsGkAtoms::selected ||
      mSelectedChanged) {
    return NS_OK;
  }
  
  // We just changed out selected state (since we look at the "selected"
  // attribute when mSelectedChanged is false.  Let's tell our select about
  // it.
  nsCOMPtr<nsISelectElement> selectInt = do_QueryInterface(GetSelect());
  if (!selectInt) {
    return NS_OK;
  }

  // Note that at this point mSelectedChanged is false and as long as that's
  // true it doesn't matter what value mIsSelected has.
  NS_ASSERTION(!mSelectedChanged, "Shouldn't be here");
  
  PRBool newSelected = (aValue != nsnull);
  PRBool inSetDefaultSelected = mIsInSetDefaultSelected;
  mIsInSetDefaultSelected = PR_TRUE;
  
  PRInt32 index;
  GetIndex(&index);
  // This should end up calling SetSelectedInternal, which we will allow to
  // take effect so that parts of SetOptionsSelectedByIndex that might depend
  // on it working don't get confused.
  rv = selectInt->SetOptionsSelectedByIndex(index, index, newSelected,
                                            PR_FALSE, PR_TRUE, aNotify,
                                            nsnull);

  // Now reset our members; when we finish the attr set we'll end up with the
  // rigt selected state.
  mIsInSetDefaultSelected = inSetDefaultSelected;
  mSelectedChanged = PR_FALSE;
  // mIsSelected doesn't matter while mSelectedChanged is false

  return rv;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetText(nsAString& aText)
{
  nsAutoString text;
  nsContentUtils::GetNodeTextContent(this, PR_FALSE, text);

  // XXX No CompressWhitespace for nsAString.  Sad.
  text.CompressWhitespace(PR_TRUE, PR_TRUE);
  aText = text;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetText(const nsAString& aText)
{
  return nsContentUtils::SetNodeTextContent(this, aText, PR_TRUE);
}

nsEventStates
nsHTMLOptionElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();
  // Nasty hack because we need to call an interface method, and one that
  // toggles some of our hidden internal state at that!  Would that we could
  // use |mutable|.
  PRBool selected;
  const_cast<nsHTMLOptionElement*>(this)->GetSelected(&selected);
  if (selected) {
    state |= NS_EVENT_STATE_CHECKED;
  }

  // Also calling a non-const interface method (for :default)
  const_cast<nsHTMLOptionElement*>(this)->GetDefaultSelected(&selected);
  if (selected) {
    state |= NS_EVENT_STATE_DEFAULT;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    state |= NS_EVENT_STATE_DISABLED;
    state &= ~NS_EVENT_STATE_ENABLED;
  } else {
    state &= ~NS_EVENT_STATE_DISABLED;
    state |= NS_EVENT_STATE_ENABLED;
  }

  return state;
}

// Get the select content element that contains this option
nsIContent*
nsHTMLOptionElement::GetSelect()
{
  nsIContent* parent = this;
  while ((parent = parent->GetParent()) &&
         parent->IsHTML()) {
    if (parent->Tag() == nsGkAtoms::select) {
      return parent;
    }
    if (parent->Tag() != nsGkAtoms::optgroup) {
      break;
    }
  }
  
  return nsnull;
}

NS_IMETHODIMP    
nsHTMLOptionElement::Initialize(nsISupports* aOwner,
                                JSContext* aContext,
                                JSObject *aObj,
                                PRUint32 argc, 
                                jsval *argv)
{
  nsresult result = NS_OK;

  if (argc > 0) {
    // The first (optional) parameter is the text of the option
    JSString* jsstr = JS_ValueToString(aContext, argv[0]);
    if (!jsstr) {
      return NS_ERROR_FAILURE;
    }

    // Create a new text node and append it to the option
    nsCOMPtr<nsIContent> textContent;
    result = NS_NewTextNode(getter_AddRefs(textContent),
                            mNodeInfo->NodeInfoManager());
    if (NS_FAILED(result)) {
      return result;
    }

    textContent->SetText(reinterpret_cast<const PRUnichar*>
                                         (JS_GetStringChars(jsstr)),
                         JS_GetStringLength(jsstr),
                         PR_FALSE);
    
    result = AppendChildTo(textContent, PR_FALSE);
    if (NS_FAILED(result)) {
      return result;
    }

    if (argc > 1) {
      // The second (optional) parameter is the value of the option
      jsstr = JS_ValueToString(aContext, argv[1]);
      if (!jsstr) {
        return NS_ERROR_FAILURE;
      }

      // Set the value attribute for this element
      nsAutoString value(reinterpret_cast<const PRUnichar*>
                                         (JS_GetStringChars(jsstr)));

      result = SetAttr(kNameSpaceID_None, nsGkAtoms::value, value,
                       PR_FALSE);
      if (NS_FAILED(result)) {
        return result;
      }

      if (argc > 2) {
        // The third (optional) parameter is the defaultSelected value
        JSBool defaultSelected;
        JS_ValueToBoolean(aContext, argv[2], &defaultSelected);
        if (defaultSelected) {
          result = SetAttr(kNameSpaceID_None, nsGkAtoms::selected,
                           EmptyString(), PR_FALSE);
          NS_ENSURE_SUCCESS(result, result);
        }

        // XXX This is *untested* behavior.  Should work though.
        if (argc > 3) {
          JSBool selected;
          JS_ValueToBoolean(aContext, argv[3], &selected);

          return SetSelected(selected);
        }
      }
    }
  }

  return result;
}

nsresult
nsHTMLOptionElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->GetOwnerDoc()->IsStaticDocument()) {
    PRBool selected = PR_FALSE;
    const_cast<nsHTMLOptionElement*>(this)->GetSelected(&selected);
    static_cast<nsHTMLOptionElement*>(aDest)->SetSelected(selected);
  }
  return NS_OK;
}

