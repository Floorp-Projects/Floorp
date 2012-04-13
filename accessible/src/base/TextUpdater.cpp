/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TextUpdater.h"

#include "Accessible-inl.h"
#include "nsDocAccessible.h"
#include "nsTextAccessible.h"

void
TextUpdater::Run(nsDocAccessible* aDocument, nsTextAccessible* aTextLeaf,
                 const nsAString& aNewText)
{
  NS_ASSERTION(aTextLeaf, "No text leaf accessible?");

  const nsString& oldText = aTextLeaf->Text();
  PRUint32 oldLen = oldText.Length(), newLen = aNewText.Length();
  PRUint32 minLen = NS_MIN(oldLen, newLen);

  // Skip coinciding begin substrings.
  PRUint32 skipStart = 0;
  for (; skipStart < minLen; skipStart++) {
    if (aNewText[skipStart] != oldText[skipStart])
      break;
  }

  // The text was changed. Do update.
  if (skipStart != minLen || oldLen != newLen) {
    TextUpdater updater(aDocument, aTextLeaf);
    updater.DoUpdate(aNewText, oldText, skipStart);
  }
}

void
TextUpdater::DoUpdate(const nsAString& aNewText, const nsAString& aOldText,
                      PRUint32 aSkipStart)
{
  nsAccessible* parent = mTextLeaf->Parent();
  if (!parent)
    return;

  mHyperText = parent->AsHyperText();
  if (!mHyperText) {
    NS_ERROR("Text leaf parent is not hypertext!");
    return;
  }

  // Get the text leaf accessible offset and invalidate cached offsets after it.
  mTextOffset = mHyperText->GetChildOffset(mTextLeaf, true);
  NS_ASSERTION(mTextOffset != -1,
               "Text leaf hasn't offset within hyper text!");

  PRUint32 oldLen = aOldText.Length(), newLen = aNewText.Length();
  PRUint32 minLen = NS_MIN(oldLen, newLen);

  // Trim coinciding substrings from the end.
  PRUint32 skipEnd = 0;
  while (minLen - skipEnd > aSkipStart &&
         aNewText[newLen - skipEnd - 1] == aOldText[oldLen - skipEnd - 1]) {
    skipEnd++;
  }

  PRUint32 strLen1 = oldLen - aSkipStart - skipEnd;
  PRUint32 strLen2 = newLen - aSkipStart - skipEnd;

  const nsAString& str1 = Substring(aOldText, aSkipStart, strLen1);
  const nsAString& str2 = Substring(aNewText, aSkipStart, strLen2);

  // Increase offset of the text leaf on skipped characters amount.
  mTextOffset += aSkipStart;

  // It could be single insertion or removal or the case of long strings. Do not
  // calculate the difference between long strings and prefer to fire pair of
  // insert/remove events as the old string was replaced on the new one.
  if (strLen1 == 0 || strLen2 == 0 ||
      strLen1 > kMaxStrLen || strLen2 > kMaxStrLen) {
    if (strLen1 > 0) {
      // Fire text change event for removal.
      nsRefPtr<AccEvent> textRemoveEvent =
        new AccTextChangeEvent(mHyperText, mTextOffset, str1, false);
      mDocument->FireDelayedAccessibleEvent(textRemoveEvent);
    }

    if (strLen2 > 0) {
      // Fire text change event for insertion.
      nsRefPtr<AccEvent> textInsertEvent =
        new AccTextChangeEvent(mHyperText, mTextOffset, str2, true);
      mDocument->FireDelayedAccessibleEvent(textInsertEvent);
    }

    mDocument->MaybeNotifyOfValueChange(mHyperText);

    // Update the text.
    mTextLeaf->SetText(aNewText);
    return;
  }

  // Otherwise find the difference between strings and fire events.
  // Note: we can skip initial and final coinciding characters since they don't
  // affect the Levenshtein distance.

  // Compute the flat structured matrix need to compute the difference.
  PRUint32 len1 = strLen1 + 1, len2 = strLen2 + 1;
  PRUint32* entries = new PRUint32[len1 * len2];

  for (PRUint32 colIdx = 0; colIdx < len1; colIdx++)
    entries[colIdx] = colIdx;

  PRUint32* row = entries;
  for (PRUint32 rowIdx = 1; rowIdx < len2; rowIdx++) {
    PRUint32* prevRow = row;
    row += len1;
    row[0] = rowIdx;
    for (PRUint32 colIdx = 1; colIdx < len1; colIdx++) {
      if (str1[colIdx - 1] != str2[rowIdx - 1]) {
        PRUint32 left = row[colIdx - 1];
        PRUint32 up = prevRow[colIdx];
        PRUint32 upleft = prevRow[colIdx - 1];
        row[colIdx] = NS_MIN(upleft, NS_MIN(left, up)) + 1;
      } else {
        row[colIdx] = prevRow[colIdx - 1];
      }
    }
  }

  // Compute events based on the difference.
  nsTArray<nsRefPtr<AccEvent> > events;
  ComputeTextChangeEvents(str1, str2, entries, events);

  delete [] entries;

  // Fire events.
  for (PRInt32 idx = events.Length() - 1; idx >= 0; idx--)
    mDocument->FireDelayedAccessibleEvent(events[idx]);

  mDocument->MaybeNotifyOfValueChange(mHyperText);

  // Update the text.
  mTextLeaf->SetText(aNewText);
}

void
TextUpdater::ComputeTextChangeEvents(const nsAString& aStr1,
                                     const nsAString& aStr2,
                                     PRUint32* aEntries,
                                     nsTArray<nsRefPtr<AccEvent> >& aEvents)
{
  PRInt32 colIdx = aStr1.Length(), rowIdx = aStr2.Length();

  // Point at which strings last matched.
  PRInt32 colEnd = colIdx;
  PRInt32 rowEnd = rowIdx;

  PRInt32 colLen = colEnd + 1;
  PRUint32* row = aEntries + rowIdx * colLen;
  PRUint32 dist = row[colIdx]; // current Levenshtein distance
  while (rowIdx && colIdx) { // stop when we can't move diagonally
    if (aStr1[colIdx - 1] == aStr2[rowIdx - 1]) { // match
      if (rowIdx < rowEnd) { // deal with any pending insertion
        FireInsertEvent(Substring(aStr2, rowIdx, rowEnd - rowIdx),
                        rowIdx, aEvents);
      }
      if (colIdx < colEnd) { // deal with any pending deletion
        FireDeleteEvent(Substring(aStr1, colIdx, colEnd - colIdx),
                        rowIdx, aEvents);
      }

      colEnd = --colIdx; // reset the match point
      rowEnd = --rowIdx;
      row -= colLen;
      continue;
    }
    --dist;
    if (dist == row[colIdx - 1 - colLen]) { // substitution
      --colIdx;
      --rowIdx;
      row -= colLen;
      continue;
    }
    if (dist == row[colIdx - colLen]) { // insertion
      --rowIdx;
      row -= colLen;
      continue;
    }
    if (dist == row[colIdx - 1]) { // deletion
      --colIdx;
      continue;
    }
    NS_NOTREACHED("huh?");
    return;
  }

  if (rowEnd)
    FireInsertEvent(Substring(aStr2, 0, rowEnd), 0, aEvents);
  if (colEnd)
    FireDeleteEvent(Substring(aStr1, 0, colEnd), 0, aEvents);
}
