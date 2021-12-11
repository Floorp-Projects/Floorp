/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleNode.h"
#include "mozilla/dom/AccessibleNodeBinding.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsContentUtils.h"
#include "nsISimpleEnumerator.h"

#include "AccAttributes.h"
#include "LocalAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "mozilla/dom/Document.h"  // for inline nsINode::GetParentObject
#include "mozilla/dom/ToJSValue.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

bool AccessibleNode::IsAOMEnabled(JSContext* aCx, JSObject* /*unused*/) {
  return nsContentUtils::IsSystemCaller(aCx) ||
         StaticPrefs::accessibility_AOM_enabled();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AccessibleNode, mRelationProperties,
                                      mIntl, mDOMNode, mStates)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AccessibleNode)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AccessibleNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AccessibleNode)

AccessibleNode::AccessibleNode(nsINode* aNode)
    : mDoubleProperties(3),
      mIntProperties(3),
      mUIntProperties(6),
      mBooleanProperties(0),
      mRelationProperties(3),
      mStringProperties(16),
      mDOMNode(aNode) {
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!accService) {
    return;
  }

  DocAccessible* doc = accService->GetDocAccessible(mDOMNode->OwnerDoc());
  if (doc) {
    mIntl = doc->GetAccessible(mDOMNode);
  }
}

AccessibleNode::~AccessibleNode() {}

/* virtual */
JSObject* AccessibleNode::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return AccessibleNode_Binding::Wrap(aCx, this, aGivenProto);
}

/* virtual */
ParentObject AccessibleNode::GetParentObject() const {
  return mDOMNode->GetParentObject();
}

void AccessibleNode::GetComputedRole(nsAString& aRole) {
  if (mIntl) {
    nsAccessibilityService* accService = GetOrCreateAccService();
    if (accService) {
      accService->GetStringRole(mIntl->Role(), aRole);
      return;
    }
  }

  aRole.AssignLiteral("unknown");
}

void AccessibleNode::GetStates(nsTArray<nsString>& aStates) {
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!mIntl || !accService) {
    aStates.AppendElement(u"defunct"_ns);
    return;
  }

  if (mStates) {
    aStates = mStates->StringArray().Clone();
    return;
  }

  mStates = accService->GetStringStates(mIntl->State());
  aStates = mStates->StringArray().Clone();
}

void AccessibleNode::GetAttributes(nsTArray<nsString>& aAttributes) {
  if (!mIntl) {
    return;
  }

  RefPtr<AccAttributes> attrs = mIntl->Attributes();

  for (auto iter : *attrs) {
    aAttributes.AppendElement(nsAtomString(iter.Name()));
  }
}

bool AccessibleNode::Is(const Sequence<nsString>& aFlavors) {
  nsAccessibilityService* accService = GetOrCreateAccService();
  if (!mIntl || !accService) {
    for (const auto& flavor : aFlavors) {
      if (!flavor.EqualsLiteral("unknown") &&
          !flavor.EqualsLiteral("defunct")) {
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

bool AccessibleNode::Has(const Sequence<nsString>& aAttributes) {
  if (!mIntl) {
    return false;
  }
  RefPtr<AccAttributes> attrs = mIntl->Attributes();
  for (const auto& attr : aAttributes) {
    RefPtr<nsAtom> attrAtom = NS_Atomize(attr);
    if (!attrs->HasAttribute(attrAtom)) {
      return false;
    }
  }
  return true;
}

void AccessibleNode::Get(JSContext* aCX, const nsAString& aAttribute,
                         JS::MutableHandle<JS::Value> aValue,
                         ErrorResult& aRv) {
  if (!mIntl) {
    aRv.ThrowInvalidStateError("No attributes available");
    return;
  }

  RefPtr<nsAtom> attrAtom = NS_Atomize(aAttribute);
  RefPtr<AccAttributes> attrs = mIntl->Attributes();
  nsAutoString valueStr;
  attrs->GetAttribute(attrAtom, valueStr);
  if (!ToJSValue(aCX, valueStr, aValue)) {
    aRv.NoteJSContextException(aCX);
    return;
  }
}

nsINode* AccessibleNode::GetDOMNode() { return mDOMNode; }
