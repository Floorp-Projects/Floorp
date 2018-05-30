/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozAutoDocUpdate_h_
#define mozAutoDocUpdate_h_

#include "nsContentUtils.h" // For AddScriptBlocker() and RemoveScriptBlocker().
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"

/**
 * Helper class to automatically handle batching of document updates.  This
 * class will call BeginUpdate on construction and EndUpdate on destruction on
 * the given document with the given update type.  The document could be null,
 * in which case no updates will be called.  The constructor also takes a
 * boolean that can be set to false to prevent notifications.
 */
class MOZ_STACK_CLASS mozAutoDocUpdate
{
public:
  mozAutoDocUpdate(nsIDocument* aDocument, bool aNotify)
    : mDocument(aNotify ? aDocument : nullptr)
  {
    if (mDocument) {
      mDocument->BeginUpdate();
    } else {
      nsContentUtils::AddScriptBlocker();
    }
  }

  ~mozAutoDocUpdate()
  {
    if (mDocument) {
      mDocument->EndUpdate();
    } else {
      nsContentUtils::RemoveScriptBlocker();
    }
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
};

#define MOZ_AUTO_DOC_UPDATE_PASTE2(tok,line) tok##line
#define MOZ_AUTO_DOC_UPDATE_PASTE(tok,line) \
  MOZ_AUTO_DOC_UPDATE_PASTE2(tok,line)
#define MOZ_AUTO_DOC_UPDATE(doc,notify) \
  mozAutoDocUpdate MOZ_AUTO_DOC_UPDATE_PASTE(_autoDocUpdater_, __LINE__) \
  (doc,notify)


#endif
