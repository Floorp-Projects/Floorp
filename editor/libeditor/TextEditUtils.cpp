/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsNameSpaceManager.h"
#include "nsLiteralString.h"
#include "nsString.h"

namespace mozilla {

/******************************************************************************
 * TextEditUtils
 ******************************************************************************/

/**
 * IsBody() returns true if aNode is an html body node.
 */
bool
TextEditUtils::IsBody(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::body);
}

/**
 * IsBreak() returns true if aNode is an html break node.
 */
bool
TextEditUtils::IsBreak(nsIDOMNode* aNode)
{
  return EditorBase::NodeIsType(aNode, nsGkAtoms::br);
}

bool
TextEditUtils::IsBreak(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::br);
}


/**
 * IsMozBR() returns true if aNode is an html br node with |type = _moz|.
 */
bool
TextEditUtils::IsMozBR(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  return IsBreak(aNode) && HasMozAttr(aNode);
}

bool
TextEditUtils::IsMozBR(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsHTMLElement(nsGkAtoms::br) &&
         aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                         NS_LITERAL_STRING("_moz"),
                                         eIgnoreCase);
}

/**
 * HasMozAttr() returns true if aNode has type attribute and its value is
 * |_moz|. (Used to indicate div's and br's we use in mail compose rules)
 */
bool
TextEditUtils::HasMozAttr(nsIDOMNode* aNode)
{
  MOZ_ASSERT(aNode);
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  if (!element) {
    return false;
  }
  nsAutoString typeAttrVal;
  nsresult rv = element->GetAttribute(NS_LITERAL_STRING("type"), typeAttrVal);
  return NS_SUCCEEDED(rv) && typeAttrVal.LowerCaseEqualsLiteral("_moz");
}

/******************************************************************************
 * AutoEditInitRulesTrigger
 ******************************************************************************/

AutoEditInitRulesTrigger::AutoEditInitRulesTrigger(TextEditor* aTextEditor,
                                                   nsresult& aResult)
  : mTextEditor(aTextEditor)
  , mResult(aResult)
{
  if (mTextEditor) {
    mTextEditor->BeginEditorInit();
  }
}

AutoEditInitRulesTrigger::~AutoEditInitRulesTrigger()
{
  if (mTextEditor) {
    mResult = mTextEditor->EndEditorInit();
  }
}

} // namespace mozilla
