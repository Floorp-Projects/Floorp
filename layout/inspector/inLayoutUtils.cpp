/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inLayoutUtils.h"

#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

///////////////////////////////////////////////////////////////////////////////

EventStateManager* inLayoutUtils::GetEventStateManagerFor(Element& aElement) {
  Document* doc = aElement.OwnerDoc();
  nsPresContext* presContext = doc->GetPresContext();
  if (!presContext) return nullptr;

  return presContext->EventStateManager();
}

Document* inLayoutUtils::GetSubDocumentFor(nsINode* aNode) {
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content) {
    nsCOMPtr<Document> doc = content->GetComposedDoc();
    if (doc) {
      return doc->GetSubDocumentFor(content);
    }
  }

  return nullptr;
}

nsINode* inLayoutUtils::GetContainerFor(const Document& aDoc) {
  nsPIDOMWindowOuter* pwin = aDoc.GetWindow();
  if (!pwin) {
    return nullptr;
  }

  return pwin->GetFrameElementInternal();
}
