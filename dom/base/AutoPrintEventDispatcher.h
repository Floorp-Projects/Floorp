/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AutoPrintEventDispatcher_h
#define mozilla_dom_AutoPrintEventDispatcher_h

#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowOuter.h"
#include "nsIPrintSettings.h"

namespace mozilla::dom {

class AutoPrintEventDispatcher {
  // NOTE(emilio): For fission iframes, we dispatch this event in
  // RecvCloneDocumentTreeIntoSelf.
  static void CollectInProcessSubdocuments(
      Document& aDoc, nsTArray<nsCOMPtr<Document>>& aDocs) {
    auto recurse = [&aDocs](Document& aSubDoc) {
      aDocs.AppendElement(&aSubDoc);
      CollectInProcessSubdocuments(aSubDoc, aDocs);
      return CallState::Continue;
    };
    aDoc.EnumerateSubDocuments(recurse);
  }

  MOZ_CAN_RUN_SCRIPT void DispatchEvent(bool aBefore) {
    for (auto& doc : mDocuments) {
      nsContentUtils::DispatchTrustedEvent(
          doc, nsGlobalWindowOuter::Cast(doc->GetWindow()),
          aBefore ? u"beforeprint"_ns : u"afterprint"_ns, CanBubble::eNo,
          Cancelable::eNo, nullptr);
      if (RefPtr<nsPresContext> presContext = doc->GetPresContext()) {
        presContext->EmulateMedium(aBefore ? nsGkAtoms::print : nullptr);
        // Ensure media query listeners fire.
        // FIXME(emilio): This is hacky, at best, but is required for compat
        // with some pages, see bug 774398.
        doc->EvaluateMediaQueriesAndReportChanges(/* aRecurse = */ false);
      }
    }
  }

 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoPrintEventDispatcher(Document& aDoc) {
    if (!aDoc.IsStaticDocument()) {
      mDocuments.AppendElement(&aDoc);
      CollectInProcessSubdocuments(aDoc, mDocuments);
    }

    DispatchEvent(true);
  }

  MOZ_CAN_RUN_SCRIPT ~AutoPrintEventDispatcher() { DispatchEvent(false); }

  AutoTArray<nsCOMPtr<Document>, 8> mDocuments;
  const nsSize mPageSize;
  nsRect mVisibleAreaToRestore;
};

}  // namespace mozilla::dom

#endif
