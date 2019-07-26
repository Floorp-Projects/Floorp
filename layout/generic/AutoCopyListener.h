/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoCopyListener_h
#define mozilla_AutoCopyListener_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/StaticPrefs_clipboard.h"
#include "nsIClipboard.h"

namespace mozilla {

class AutoCopyListener final {
 public:
  /**
   * OnSelectionChange() is called when a Selection whose NotifyAutoCopy() was
   * called is changed.
   *
   * @param aDocument           The document of the Selection.  May be nullptr.
   * @param aSelection          The selection.
   * @param aReason             The reasons of the change.
   *                            See nsISelectionListener::*_REASON.
   */
  static void OnSelectionChange(dom::Document* aDocument,
                                dom::Selection& aSelection, int16_t aReason);

  /**
   * Init() initializes all static members of this class.  Should be called
   * only once.
   */
  static void Init(int16_t aClipboardID) {
    MOZ_ASSERT(IsValidClipboardID(aClipboardID));
    static bool sInitialized = false;
    if (!sInitialized && IsValidClipboardID(aClipboardID)) {
      sClipboardID = aClipboardID;
      sInitialized = true;
    }
  }

  /**
   * IsPrefEnabled() returns true if the pref enables auto-copy feature.
   */
  static bool IsPrefEnabled() { return StaticPrefs::clipboard_autocopy(); }

 private:
  static bool IsValidClipboardID(int16_t aClipboardID) {
    return aClipboardID >= nsIClipboard::kSelectionClipboard &&
           aClipboardID <= nsIClipboard::kSelectionCache;
  }

  static int16_t sClipboardID;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_AutoCopyListener_h
