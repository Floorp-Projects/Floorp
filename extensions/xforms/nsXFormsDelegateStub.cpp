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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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


#include "nsIDOMDocument.h"
#include "nsIDOM3Node.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMDocumentEvent.h"
#include "nsDOMString.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsUIWidget.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsDelegateStub.h"
#include "nsXFormsUtils.h"
#include "nsIServiceManager.h"
#include "nsXFormsModelElement.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsDelegateStub,
                             nsXFormsControlStub,
                             nsIDelegateInternal,
                             nsIXFormsDelegate)


NS_IMETHODIMP
nsXFormsDelegateStub::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  mRepeatState = eType_Unknown;
  return nsXFormsControlStub::WillChangeDocument(aNewDocument);
}

NS_IMETHODIMP
nsXFormsDelegateStub::WillChangeParent(nsIDOMElement *aNewParent)
{
  mRepeatState = eType_Unknown;
  return nsXFormsControlStub::WillChangeParent(aNewParent);
}

NS_IMETHODIMP
nsXFormsDelegateStub::GetAccesskeyNode(nsIDOMAttr** aNode)
{
  return mElement->GetAttributeNode(NS_LITERAL_STRING("accesskey"), aNode);
}

NS_IMETHODIMP
nsXFormsDelegateStub::PerformAccesskey()
{
  nsCOMPtr<nsIXFormsUIWidget> widget(do_QueryInterface(mElement));
  if (widget) {
    PRBool isFocused;
    widget->Focus(&isFocused);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsControlStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);
  aWrapper->SetNotificationMask(kStandardNotificationMask |
                                nsIXTFElement::NOTIFY_WILL_CHANGE_PARENT |
                                nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY);
  return rv;
}

NS_IMETHODIMP
nsXFormsDelegateStub::OnDestroyed()
{
  nsXFormsModelElement::CancelPostRefresh(this);
  if (mAccessor) {
    mAccessor->Destroy();
  }
  return nsXFormsControlStub::OnDestroyed();
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsDelegateStub::Refresh()
{
  if (mRepeatState == eType_Template)
    return NS_OK_XFORMS_NOREFRESH;

  const nsVoidArray* list = nsPostRefresh::PostRefreshList();
  if (list && list->IndexOf(this) >= 0) {
    // This control will be refreshed later.
    return NS_OK_XFORMS_NOREFRESH;
  }

  nsresult rv = nsXFormsControlStub::Refresh();
  NS_ENSURE_SUCCESS(rv, rv);

  SetMozTypeAttribute();

  nsCOMPtr<nsIXFormsUIWidget> widget = do_QueryInterface(mElement);

  return widget ? widget->Refresh() : NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  if (GetRelevantState()) {
    nsCOMPtr<nsIXFormsUIWidget> widget = do_QueryInterface(mElement);
    if (widget) {
      widget->Focus(aOK);
    }
  }

  return NS_OK;
}

// nsIXFormsDelegate

NS_IMETHODIMP
nsXFormsDelegateStub::GetValue(nsAString& aValue)
{
  SetDOMStringToNull(aValue);
  if (mBoundNode) {
    nsXFormsUtils::GetNodeValue(mBoundNode, aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::SetValue(const nsAString& aValue)
{
  if (!mBoundNode || !mModel)
    return NS_OK;

  PRBool changed;
  nsresult rv = mModel->SetNodeValue(mBoundNode, aValue, PR_TRUE, &changed);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::GetHasBoundNode(PRBool *aHasBoundNode)
{
  NS_ENSURE_ARG_POINTER(aHasBoundNode);
  *aHasBoundNode = mBoundNode ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::ReportError(const nsAString& aErrorMsg)
{
  const nsPromiseFlatString& flat = PromiseFlatString(aErrorMsg);
  nsXFormsUtils::ReportError(flat, mElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::ReportErrorMessage(const nsAString& aErrorMsg)
{
  const nsPromiseFlatString& flat = PromiseFlatString(aErrorMsg);
  nsXFormsUtils::ReportErrorMessage(flat, mElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsDelegateStub::WidgetAttached()
{
  if (UpdateRepeatState() == eType_Template)
    return NS_OK;

  if (HasBindingAttribute()) {
    // If control is bounded to instance data then we should ask for refresh
    // only when model is loaded entirely. The reason is control is refreshed
    // by model when it get loaded.
    if (!nsXFormsUtils::IsDocumentReadyForBind(mElement))
      return NS_OK;
  }

  nsXFormsModelElement::NeedsPostRefresh(this);
  return NS_OK;
}

// nsXFormsDelegateStub

NS_IMETHODIMP
nsXFormsDelegateStub::IsTypeAllowed(PRUint16 aType, PRBool *aIsAllowed,
                                    nsRestrictionFlag *aRestriction,
                                    nsAString &aTypes)
{
  NS_ENSURE_ARG_POINTER(aRestriction);
  NS_ENSURE_ARG_POINTER(aIsAllowed);
  *aIsAllowed = PR_TRUE;
  *aRestriction = eTypes_NoRestriction;
  aTypes.Truncate();
  return NS_OK;
}

nsRepeatState
nsXFormsDelegateStub::UpdateRepeatState()
{
  mRepeatState = eType_NotApplicable;
  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  while (parent) {
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("contextcontainer"))) {
      mRepeatState = eType_GeneratedContent;
      break;
    }
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("repeat"))) {
      mRepeatState = eType_Template;
      break;
    }
    if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("itemset"))) {
      mRepeatState = eType_Template;
      break;
    }
    nsCOMPtr<nsIDOMNode> tmp;
    parent->GetParentNode(getter_AddRefs(tmp));
    parent = tmp;
  }
  return mRepeatState;
}

void
nsXFormsDelegateStub::SetMozTypeAttribute()
{
  NS_NAMED_LITERAL_STRING(mozTypeNs, NS_NAMESPACE_MOZ_XFORMS_TYPE);
  NS_NAMED_LITERAL_STRING(mozType, "type");
  NS_NAMED_LITERAL_STRING(mozTypeList, "typelist");
  NS_NAMED_LITERAL_STRING(mozRejectedType, "rejectedtype");

  // can't use mBoundNode here since mBoundNode can exist for xf:output, for
  // example, even if there is no binding attribute.
  nsCOMPtr<nsIDOMNode> boundNode;
  GetBoundNode(getter_AddRefs(boundNode));
  if (mModel && boundNode) {
    nsAutoString type, nsOrig;
    if (NS_FAILED(mModel->GetTypeAndNSFromNode(boundNode, type, nsOrig))) {
      mElement->RemoveAttributeNS(mozTypeNs, mozType);
      mElement->RemoveAttributeNS(mozTypeNs, mozTypeList);
      mElement->RemoveAttributeNS(mozTypeNs, mozRejectedType);
      return;
    }

    nsAutoString attrValue(nsOrig);
    attrValue.AppendLiteral("#");
    attrValue.Append(type);
    mElement->SetAttributeNS(mozTypeNs, mozType, attrValue);

    // Get the list of types that this type derives from and set it as the
    // value of the basetype attribute
    nsresult rv = mModel->GetDerivedTypeList(type, nsOrig, attrValue);
    if (NS_SUCCEEDED(rv)) {
      mElement->SetAttributeNS(mozTypeNs, mozTypeList, attrValue);
    } else {
      // Note: even if we can't build the derived type list, we should leave on
      // mozType attribute.  We can still use the attr for validation, etc.  But
      // the type-restricted controls like range and upload won't display since
      // they are bound to their anonymous content by @typeList.  Make sure that
      // we don't leave around mozTypeList if it isn't accurate.
      mElement->RemoveAttributeNS(mozTypeNs, mozTypeList);
    }

    // Get the builtin type that the bound type is derived from.  Then determine
    // if this control is allowed to bind to this type.  Some controls like
    // input, secret, textarea, upload and range can only bind to some types
    // and not to others.
    PRUint16 builtinType = 0;
    rv = GetBoundBuiltinType(&builtinType);
    if (NS_SUCCEEDED(rv)) {
      PRBool isAllowed = PR_TRUE;
      nsAutoString restrictedTypeList;
      nsRestrictionFlag restriction;
      IsTypeAllowed(builtinType, &isAllowed, &restriction, restrictedTypeList);
      if (!isAllowed) {
        // if this control isn't allowed to bind to this type, we'll set the
        // 'mozType:rejectedtype' attr to true so that our default CSS will
        // not display the control
        mElement->SetAttributeNS(mozTypeNs, mozRejectedType,
                                 NS_LITERAL_STRING("true"));

        // build the error string that we want output to the ErrorConsole
        nsAutoString localName;
        mElement->GetLocalName(localName);
        const PRUnichar *strings[] = { localName.get(), restrictedTypeList.get() };

        nsXFormsUtils::ReportError(
          restriction == eTypes_Inclusive ?
            NS_LITERAL_STRING("boundTypeErrorInclusive") :
            NS_LITERAL_STRING("boundTypeErrorExclusive"),
          strings, 2, mElement, mElement);
        return;
      }
    }
    // We reached here for one of two reasons:
    // 1) The control can handle this type
    // 2) We don't have enough information to make a judgement.
    //
    // Either way, we'll remove the attribute so that the control is useable
    mElement->RemoveAttributeNS(mozTypeNs, mozRejectedType);

    return;
  }

  mElement->RemoveAttributeNS(mozTypeNs, mozType);
  mElement->RemoveAttributeNS(mozTypeNs, mozTypeList);
  mElement->RemoveAttributeNS(mozTypeNs, mozRejectedType);
  return;
}

NS_IMETHODIMP
nsXFormsDelegateStub::GetXFormsAccessors(nsIXFormsAccessors **aAccessor)
{
  if (!mAccessor) {
    mAccessor = new nsXFormsAccessors(this, mElement);
    if (!mAccessor) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ADDREF(*aAccessor = mAccessor);
  return NS_OK;
}
