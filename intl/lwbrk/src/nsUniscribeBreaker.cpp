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

void
NS_GetComplexLineBreaks(const PRUnichar* aText, PRUint32 aLength,
                        PRUint8* aBreakBefore)
{
  NS_ASSERTION(aText, "aText shouldn't be null"); 

  int outItems = 0;
  HRESULT result;
  nsAutoTArray<SCRIPT_ITEM, 64> items;

  memset(aBreakBefore, false, aLength);

  if (!items.AppendElements(64))
    return;

  do {
    result = ScriptItemize(aText, aLength, items.Length(), NULL, NULL, items.Elements(), &outItems);

    if (result == E_OUTOFMEMORY) {
      if (!items.AppendElements(items.Length()))
        return;
    }
  } while (result == E_OUTOFMEMORY);

  for (int iItem = 0; iItem < outItems; ++iItem)  {
    PRUint32 endOffset = (iItem + 1 == outItems ? aLength : items[iItem + 1].iCharPos);
    PRUint32 startOffset = items[iItem].iCharPos;
    nsAutoTArray<SCRIPT_LOGATTR, 64> sla;
    
    if (!sla.AppendElements(endOffset - startOffset))
      return;

    if (ScriptBreak(aText + startOffset, endOffset - startOffset,
                    &items[iItem].a,  sla.Elements()) < 0) 
      return;

    for (PRUint32 j=0; j+startOffset < endOffset; ++j) {
       aBreakBefore[j+startOffset] = sla[j].fSoftBreak;
    }
  }
}
