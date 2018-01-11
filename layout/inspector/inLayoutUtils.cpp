/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inLayoutUtils.h"

#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

///////////////////////////////////////////////////////////////////////////////

EventStateManager*
inLayoutUtils::GetEventStateManagerFor(Element& aElement)
{
  nsIDocument* doc = aElement.OwnerDoc();
  nsIPresShell* shell = doc->GetShell();
  if (!shell)
    return nullptr;

  return shell->GetPresContext()->EventStateManager();
}

nsIDOMDocument*
inLayoutUtils::GetSubDocumentFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content) {
    nsCOMPtr<nsIDocument> doc = content->GetComposedDoc();
    if (doc) {
      nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc->GetSubDocumentFor(content)));

      return domdoc;
    }
  }

  return nullptr;
}

nsINode*
inLayoutUtils::GetContainerFor(const nsIDocument& aDoc)
{
  nsPIDOMWindowOuter* pwin = aDoc.GetWindow();
  if (!pwin) {
    return nullptr;
  }

  return pwin->GetFrameElementInternal();
}

