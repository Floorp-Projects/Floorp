/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsNameSpaceManager.h"
#include "nsLiteralString.h"
#include "nsPlaintextEditor.h"
#include "nsString.h"
#include "nsTextEditUtils.h"

using namespace mozilla;

///////////////////////////////////////////////////////////////////////////
// IsBody: true if node an html body node
bool 
nsTextEditUtils::IsBody(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsGkAtoms::body);
}


///////////////////////////////////////////////////////////////////////////
// IsBreak: true if node an html break node
bool 
nsTextEditUtils::IsBreak(nsIDOMNode *node)
{
  return nsEditor::NodeIsType(node, nsGkAtoms::br);
}


///////////////////////////////////////////////////////////////////////////
// IsMozBR: true if node an html br node with type = _moz
//                  
bool 
nsTextEditUtils::IsMozBR(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null node passed to nsHTMLEditUtils::IsMozBR");
  return IsBreak(node) && HasMozAttr(node);
}


bool
nsTextEditUtils::IsMozBR(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  return aNode->IsElement() &&
         aNode->AsElement()->IsHTML(nsGkAtoms::br) &&
         aNode->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                         NS_LITERAL_STRING("_moz"),
                                         eIgnoreCase);
}

///////////////////////////////////////////////////////////////////////////
// HasMozAttr: true if node has type attribute = _moz
//             (used to indicate the div's and br's we use in
//              mail compose rules)
//                  
bool 
nsTextEditUtils::HasMozAttr(nsIDOMNode *node)
{
  NS_PRECONDITION(node, "null parent passed to nsHTMLEditUtils::HasMozAttr");
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(node);
  if (elem)
  {
    nsAutoString typeAttrVal;
    nsresult res = elem->GetAttribute(NS_LITERAL_STRING("type"), typeAttrVal);
    if (NS_SUCCEEDED(res) && (typeAttrVal.LowerCaseEqualsLiteral("_moz")))
      return true;
  }
  return false;
}


///////////////////////////////////////////////////////////////////////////
// nsAutoEditInitRulesTrigger methods
//
nsAutoEditInitRulesTrigger::nsAutoEditInitRulesTrigger( nsPlaintextEditor *aEd, nsresult &aRes) : mEd(aEd), mRes(aRes)
{
    if (mEd) mEd->BeginEditorInit();
}

nsAutoEditInitRulesTrigger::~nsAutoEditInitRulesTrigger()
{
    if (mEd) mRes = mEd->EndEditorInit();
}

