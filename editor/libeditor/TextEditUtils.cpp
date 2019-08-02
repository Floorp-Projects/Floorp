/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsLiteralString.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

/******************************************************************************
 * TextEditUtils
 ******************************************************************************/

/**
 * IsBody() returns true if aNode is an html body node.
 */
bool TextEditUtils::IsBody(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::body);
}

/**
 * IsBreak() returns true if aNode is an html break node.
 */
bool TextEditUtils::IsBreak(nsINode* aNode) {
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::br);
}

/******************************************************************************
 * AutoEditInitRulesTrigger
 ******************************************************************************/

AutoEditInitRulesTrigger::AutoEditInitRulesTrigger(TextEditor* aTextEditor,
                                                   nsresult& aResult)
    : mTextEditor(aTextEditor), mResult(aResult) {
  if (mTextEditor) {
    mTextEditor->BeginEditorInit();
  }
}

AutoEditInitRulesTrigger::~AutoEditInitRulesTrigger() {
  if (mTextEditor) {
    mResult = MOZ_KnownLive(mTextEditor)->EndEditorInit();
  }
}

}  // namespace mozilla
