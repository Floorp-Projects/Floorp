/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleNode.h"
#include "mozilla/dom/AccessibleNodeBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(AccessibleNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AccessibleNode)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AccessibleNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AccessibleNode)

AccessibleNode::AccessibleNode(nsINode* aNode) : mDOMNode(aNode)
{
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!accService) {
    return;
  }

  DocAccessible* doc = accService->GetDocAccessible(mDOMNode->OwnerDoc());
  if (doc) {
    mIntl = doc->GetAccessible(mDOMNode);
  }
}

AccessibleNode::~AccessibleNode()
{
}

/* virtual */ JSObject*
AccessibleNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AccessibleNodeBinding::Wrap(aCx, this, aGivenProto);
}

/* virtual */ ParentObject
AccessibleNode::GetParentObject() const
{
  return mDOMNode->GetParentObject();
}

void
AccessibleNode::GetRole(nsAString& aRole)
{
  if (mIntl) {
    nsAccessibilityService* accService = GetOrCreateAccService();
    if (accService) {
      accService->GetStringRole(mIntl->Role(), aRole);
      return;
    }
  }

  aRole.AssignLiteral("unknown");
}

void
AccessibleNode::GetStates(nsTArray<nsString>& aStates)
{
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!mIntl || !accService) {
    aStates.AppendElement(NS_LITERAL_STRING("defunct"));
    return;
  }

  if (mStates) {
    aStates = mStates->StringArray();
    return;
  }

  mStates = accService->GetStringStates(mIntl->State());
  aStates = mStates->StringArray();
}

void
AccessibleNode::GetAttributes(nsTArray<nsString>& aAttributes)
{
  if (!mIntl) {
    return;
  }

  nsCOMPtr<nsIPersistentProperties> attrs = mIntl->Attributes();

  nsCOMPtr<nsISimpleEnumerator> props;
  attrs->Enumerate(getter_AddRefs(props));

  bool hasMore = false;
  while (NS_SUCCEEDED(props->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supp;
    props->GetNext(getter_AddRefs(supp));

    nsCOMPtr<nsIPropertyElement> prop(do_QueryInterface(supp));

    nsAutoCString attr;
    prop->GetKey(attr);
    aAttributes.AppendElement(NS_ConvertUTF8toUTF16(attr));
  }
}

bool
AccessibleNode::Is(const Sequence<nsString>& aFlavors)
{
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!mIntl || !accService) {
    for (const auto& flavor : aFlavors) {
      if (!flavor.EqualsLiteral("unknown") && !flavor.EqualsLiteral("defunct")) {
        return false;
      }
    }
    return true;
  }

  nsAutoString role;
  accService->GetStringRole(mIntl->Role(), role);

  if (!mStates) {
    mStates = accService->GetStringStates(mIntl->State());
  }

  for (const auto& flavor : aFlavors) {
    if (!flavor.Equals(role) && !mStates->Contains(flavor)) {
      return false;
    }
  }
  return true;
}

bool
AccessibleNode::Has(const Sequence<nsString>& aAttributes)
{
  if (!mIntl) {
    return false;
  }
  nsCOMPtr<nsIPersistentProperties> attrs = mIntl->Attributes();
  for (const auto& attr : aAttributes) {
    bool has = false;
    attrs->Has(NS_ConvertUTF16toUTF8(attr).get(), &has);
    if (!has) {
      return false;
    }
  }
  return true;
}

void
AccessibleNode::Get(JSContext* aCX, const nsAString& aAttribute,
                    JS::MutableHandle<JS::Value> aValue,
                    ErrorResult& aRv)
{
  if (!mIntl) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCOMPtr<nsIPersistentProperties> attrs = mIntl->Attributes();
  nsAutoString value;
  attrs->GetStringProperty(NS_ConvertUTF16toUTF8(aAttribute), value);

  JS::Rooted<JS::Value> jsval(aCX);
  if (!ToJSValue(aCX, value, &jsval)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aValue.set(jsval);
}

nsINode*
AccessibleNode::GetDOMNode()
{
  return mDOMNode;
}
