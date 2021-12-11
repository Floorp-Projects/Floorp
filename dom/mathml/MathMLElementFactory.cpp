/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/MathMLElement.h"

using namespace mozilla::dom;

// MathML Element Factory (declared in nsContentCreatorFunctions.h)
nsresult NS_NewMathMLElement(
    Element** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  NS_ADDREF(*aResult = new (nim) MathMLElement(nodeInfo.forget()));
  return NS_OK;
}
