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
  static void Run(nsDocAccessible* aDocument, nsTextAccessible* aTextLeaf,
                  const nsAString& aNewText);

private:
  TextUpdater(nsDocAccessible* aDocument, nsTextAccessible* aTextLeaf) :
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
                             aText, PR_TRUE);
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
                             aText, PR_FALSE);
    aEvents.AppendElement(event);
  }

  /**
   * The constant used to skip string difference calculation in case of long
   * strings.
   */
  const static PRUint32 kMaxStrLen = 1 << 6;

private:
  nsDocAccessible* mDocument;
  nsTextAccessible* mTextLeaf;
  nsHyperTextAccessible* mHyperText;
  PRInt32 mTextOffset;
};

#endif
