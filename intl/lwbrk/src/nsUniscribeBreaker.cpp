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
 * Pattara Kiatisevi <ott@linux.thai.net>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * - Nattachai Ungsriwong <nattachai@gmail.com>
 * - Theppitak Karoonboonyanan <thep@linux.thai.net>
 * - Vee Satayamas <vsatayamas@gmail.com> 
 * - Pattara Kiatisevi <ott@linux.thai.net>
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

  memset(aBreakBefore, PR_FALSE, aLength);

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
