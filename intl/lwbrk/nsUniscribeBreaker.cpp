/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComplexBreaker.h"

#include <windows.h>

#include <usp10.h>

#include "nsUTF8Utils.h"
#include "nsString.h"
#include "nsTArray.h"

void NS_GetComplexLineBreaks(const char16_t* aText, uint32_t aLength,
                             uint8_t* aBreakBefore) {
  NS_ASSERTION(aText, "aText shouldn't be null");

  int outItems = 0;
  HRESULT result;
  AutoTArray<SCRIPT_ITEM, 64> items;
  char16ptr_t text = aText;

  memset(aBreakBefore, false, aLength);

  items.AppendElements(64);

  do {
    result = ScriptItemize(text, aLength, items.Length(), nullptr, nullptr,
                           items.Elements(), &outItems);

    if (result == E_OUTOFMEMORY) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      items.AppendElements(items.Length());
    }
  } while (result == E_OUTOFMEMORY);

  for (int iItem = 0; iItem < outItems; ++iItem) {
    uint32_t endOffset =
        (iItem + 1 == outItems ? aLength : items[iItem + 1].iCharPos);
    uint32_t startOffset = items[iItem].iCharPos;
    AutoTArray<SCRIPT_LOGATTR, 64> sla;

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    sla.AppendElements(endOffset - startOffset);

    if (ScriptBreak(text + startOffset, endOffset - startOffset,
                    &items[iItem].a, sla.Elements()) < 0)
      return;

    // We don't want to set a potential break position at the start of text;
    // that's the responsibility of a higher level.
    for (uint32_t j = startOffset ? 0 : 1; j + startOffset < endOffset; ++j) {
      aBreakBefore[j + startOffset] = sla[j].fSoftBreak;
    }
  }
}
