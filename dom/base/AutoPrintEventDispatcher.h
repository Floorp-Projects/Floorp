/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AutoPrintEventDispatcher_h
#define mozilla_dom_AutoPrintEventDispatcher_h

#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

class AutoPrintEventDispatcher {
  // NOTE(emilio): For fission iframes, we dispatch this event in
  // RecvCloneDocumentTreeIntoSelf.
  static void CollectInProcessSubdocuments(
      Document& aDoc, nsTArray<nsCOMPtr<Document>>& aDocs) {
    aDocs.AppendElement(&aDoc);
    auto recurse = [&aDocs](Document& aSubDoc) {
      CollectInProcessSubdocuments(aSubDoc, aDocs);
      return CallState::Continue;
    };
    aDoc.EnumerateSubDocuments(recurse);
  }

  void DispatchEvent(bool aBefore) {
    for (nsCOMPtr<Document>& doc : mDocuments) {
      nsContentUtils::DispatchTrustedEvent(
          doc, doc->GetWindow(), aBefore ? u"beforeprint"_ns : u"afterprint"_ns,
          CanBubble::eNo, Cancelable::eNo, nullptr);
    }
  }

 public:
  explicit AutoPrintEventDispatcher(Document& aDoc) {
    if (!aDoc.IsStaticDocument()) {
      CollectInProcessSubdocuments(aDoc, mDocuments);
    }

    DispatchEvent(true);
  }

  ~AutoPrintEventDispatcher() { DispatchEvent(false); }

  AutoTArray<nsCOMPtr<Document>, 8> mDocuments;
};

}  // namespace mozilla::dom

#endif
