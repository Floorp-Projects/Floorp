/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextUpdater_h_
#define TextUpdater_h_

#include "AccEvent.h"
#include "nsHyperTextAccessible.h"

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
  static void Run(DocAccessible* aDocument,
                  mozilla::a11y::TextLeafAccessible* aTextLeaf,
                  const nsAString& aNewText);

private:
  TextUpdater(DocAccessible* aDocument,
              mozilla::a11y::TextLeafAccessible* aTextLeaf) :
    mDocument(aDocument), mTextLeaf(aTextLeaf), mHyperText(nsnull),
    mTextOffset(-1) { }

  ~TextUpdater()
    { mDocument = nsnull; mTextLeaf = nsnull; mHyperText = nsnull; }

  /**
   * Update text of the text leaf accessible, fire text change and value change
   * (if applicable) events for its container hypertext accessible.
   */
  void DoUpdate(const nsAString& aNewText, const nsAString& aOldText,
                PRUint32 aSkipStart);

private:
  TextUpdater();
  TextUpdater(const TextUpdater&);
  TextUpdater& operator=(const TextUpdater&);

  /**
   * Fire text change events based on difference between strings.
   */
  void ComputeTextChangeEvents(const nsAString& aStr1,
                               const nsAString& aStr2,
                               PRUint32* aEntries,
                               nsTArray<nsRefPtr<AccEvent> >& aEvents);

  /**
   * Helper to create text change events for inserted text.
   */
  inline void FireInsertEvent(const nsAString& aText, PRUint32 aAddlOffset,
                              nsTArray<nsRefPtr<AccEvent> >& aEvents)
  {
    nsRefPtr<AccEvent> event =
      new AccTextChangeEvent(mHyperText, mTextOffset + aAddlOffset,
                             aText, true);
    aEvents.AppendElement(event);
  }

  /**
   * Helper to create text change events for removed text.
   */
  inline void FireDeleteEvent(const nsAString& aText, PRUint32 aAddlOffset,
                              nsTArray<nsRefPtr<AccEvent> >& aEvents)
  {
    nsRefPtr<AccEvent> event =
      new AccTextChangeEvent(mHyperText, mTextOffset + aAddlOffset,
                             aText, false);
    aEvents.AppendElement(event);
  }

  /**
   * The constant used to skip string difference calculation in case of long
   * strings.
   */
  const static PRUint32 kMaxStrLen = 1 << 6;

private:
  DocAccessible* mDocument;
  mozilla::a11y::TextLeafAccessible* mTextLeaf;
  nsHyperTextAccessible* mHyperText;
  PRInt32 mTextOffset;
};

#endif
