/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleNode.h"
#include "mozilla/dom/AccessibleNodeBinding.h"
#include "mozilla/dom/BindingDeclarations.h"

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
  DocAccessible* doc =
    GetOrCreateAccService()->GetDocAccessible(mDOMNode->OwnerDoc());
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
    GetOrCreateAccService()->GetStringRole(mIntl->Role(), aRole);
    return;
  }

  aRole.AssignLiteral("unknown");
}

nsINode*
AccessibleNode::GetDOMNode()
{
  return mDOMNode;
}
