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
#include "nsIPrintSettings.h"

namespace mozilla::dom {

class AutoPrintEventDispatcher {
  // NOTE(emilio): For fission iframes, we dispatch this event in
  // RecvCloneDocumentTreeIntoSelf.
  static void CollectInProcessSubdocuments(
      Document& aDoc, nsTArray<std::pair<nsCOMPtr<Document>, bool>>& aDocs) {
    auto recurse = [&aDocs](Document& aSubDoc) {
      aDocs.AppendElement(std::make_pair(&aSubDoc, false));
      CollectInProcessSubdocuments(aSubDoc, aDocs);
      return CallState::Continue;
    };
    aDoc.EnumerateSubDocuments(recurse);
  }

  MOZ_CAN_RUN_SCRIPT void DispatchEvent(bool aBefore) {
    for (auto& [doc, isTop] : mDocuments) {
      nsContentUtils::DispatchTrustedEvent(
          doc, doc->GetWindow(), aBefore ? u"beforeprint"_ns : u"afterprint"_ns,
          CanBubble::eNo, Cancelable::eNo, nullptr);
      if (RefPtr<nsPresContext> presContext = doc->GetPresContext()) {
        presContext->EmulateMedium(aBefore ? nsGkAtoms::print : nullptr);
        bool needResizeEvent = false;
        if (isTop) {
          // NOTE(emilio): Having this code here means that we only fire the
          // resize on the initial document clone, not on any further print
          // settings changes. This is an intentional trade-off.
          //
          // On a continuously changing document, we don't want to keep
          // re-cloning every time the user changes the print settings, as it
          // changes what you're printing.
          if (aBefore) {
            mVisibleAreaToRestore = presContext->GetVisibleArea();
            presContext->SetVisibleArea(
                nsRect(mVisibleAreaToRestore.TopLeft(), mPageSize));
            needResizeEvent = mVisibleAreaToRestore.Size() != mPageSize;
          } else {
            if (presContext->GetVisibleArea().Size() == mPageSize) {
              presContext->SetVisibleArea(mVisibleAreaToRestore);
              needResizeEvent = mVisibleAreaToRestore.Size() != mPageSize;
            } else {
              // This shouldn't happen, but can if used by privileged script,
              // like in test_printpreview.xhtml. It's ok to do nothing here
              // tho, we just need to make sure that the mPageSize viewport size
              // doesn't persist.
              NS_WARNING(
                  "Something changed our viewport size between firing "
                  "before/afterprint?");
            }
          }
        }

        // Ensure media query listeners fire.
        doc->FlushPendingNotifications(FlushType::Style);

        if (needResizeEvent) {
          if (RefPtr ps = presContext->GetPresShell()) {
            ps->FireResizeEventSync();
          }
        }
      }
    }
  }

  static nsSize ComputePrintPageSize(nsIPrintSettings* aPrintSettings) {
    double pageWidth = 0;
    double pageHeight = 0;
    aPrintSettings->GetEffectivePageSize(&pageWidth, &pageHeight);
    return nsSize(nsPresContext::CSSTwipsToAppUnits(NSToIntFloor(pageWidth)),
                  nsPresContext::CSSTwipsToAppUnits(NSToIntFloor(pageHeight)));
  }

 public:
  MOZ_CAN_RUN_SCRIPT AutoPrintEventDispatcher(Document& aDoc,
                                              nsIPrintSettings* aPrintSettings,
                                              bool aIsTop)
      : mPageSize(ComputePrintPageSize(aPrintSettings)) {
    if (!aDoc.IsStaticDocument()) {
      mDocuments.AppendElement(std::make_pair(&aDoc, aIsTop));
      CollectInProcessSubdocuments(aDoc, mDocuments);
    }

    DispatchEvent(true);
  }

  MOZ_CAN_RUN_SCRIPT ~AutoPrintEventDispatcher() { DispatchEvent(false); }

  // The bool represents whether this is the top-level paginated document.
  AutoTArray<std::pair<nsCOMPtr<Document>, bool>, 8> mDocuments;
  const nsSize mPageSize;
  nsRect mVisibleAreaToRestore;
};

}  // namespace mozilla::dom

#endif
