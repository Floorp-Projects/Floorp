/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of non-inline bits of nsICSSDeclaration. */

#include "nsICSSDeclaration.h"

#include "nsINode.h"

using mozilla::dom::DocGroup;

DocGroup* nsICSSDeclaration::GetDocGroup() {
  nsINode* parentNode = GetParentObject();
  if (!parentNode) {
    return nullptr;
  }

  return parentNode->GetDocGroup();
}

bool nsICSSDeclaration::IsReadOnly() {
  mozilla::css::Rule* rule = GetParentRule();
  return rule && rule->IsReadOnly();
}
