/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextUpdater_h__
#define mozilla_a11y_TextUpdater_h__

#include "AccEvent.h"
#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used to find a difference between old and new text and fire text change
 * events.
 */
class TextUpdater
{
public:
  /**
   * Start text of the text leaf update.
   */
  static void Run(DocAccessible* aDocument, TextLeafAccessible* aTextLeaf,
                  const nsAString& aNewText);

private:
  TextUpdater(DocAccessible* aDocument, TextLeafAccessible* aTextLeaf) :
    mDocument(aDocument), mTextLeaf(aTextLeaf), mHyperText(nullptr),
    mTextOffset(-1) { }

  ~TextUpdater()
    { mDocument = nullptr; mTextLeaf = nullptr; mHyperText = nullptr; }

  /**
   * Update text of the text leaf accessible, fire text change and value change
   * (if applicable) events for its container hypertext accessible.
   */
  void DoUpdate(const nsAString& aNewText, const nsAString& aOldText,
                uint32_t aSkipStart);

private:
  TextUpdater();
  TextUpdater(const TextUpdater&);
  TextUpdater& operator=(const TextUpdater&);

  /**
   * Fire text change events based on difference between strings.
   */
  void ComputeTextChangeEvents(const nsAString& aStr1,
                               const nsAString& aStr2,
                               uint32_t* aEntries,
                               nsTArray<RefPtr<AccEvent> >& aEvents);

  /**
   * Helper to create text change events for inserted text.
   */
  inline void FireInsertEvent(const nsAString& aText, uint32_t aAddlOffset,
                              nsTArray<RefPtr<AccEvent> >& aEvents)
  {
    RefPtr<AccEvent> event =
      new AccTextChangeEvent(mHyperText, mTextOffset + aAddlOffset,
                             aText, true);
    aEvents.AppendElement(event);
  }

  /**
   * Helper to create text change events for removed text.
   */
  inline void FireDeleteEvent(const nsAString& aText, uint32_t aAddlOffset,
                              nsTArray<RefPtr<AccEvent> >& aEvents)
  {
    RefPtr<AccEvent> event =
      new AccTextChangeEvent(mHyperText, mTextOffset + aAddlOffset,
                             aText, false);
    aEvents.AppendElement(event);
  }

  /**
   * The constant used to skip string difference calculation in case of long
   * strings.
   */
  const static uint32_t kMaxStrLen = 1 << 6;

private:
  DocAccessible* mDocument;
  TextLeafAccessible* mTextLeaf;
  HyperTextAccessible* mHyperText;
  int32_t mTextOffset;
};

} // namespace a11y
} // namespace mozilla

#endif
