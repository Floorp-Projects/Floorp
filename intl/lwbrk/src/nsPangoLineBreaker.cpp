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
 * Theppitak Karoonboonyanan <thep@linux.thai.net>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * - Theppitak Karoonboonyanan <thep@linux.thai.net>
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



#include "nsPangoLineBreaker.h"

#include <pango/pango.h>

#include "nsLWBRKDll.h"
#include "nsUnicharUtils.h"
#include "nsUTF8Utils.h"
#include "nsString.h"
#include "nsTArray.h"


NS_IMPL_ISUPPORTS1(nsPangoLineBreaker, nsILineBreaker)

PRBool
nsPangoLineBreaker::BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                                   const PRUnichar* aText2 , PRUint32 aTextLen2)
{
  if (!aText1 || !aText2 || (0 == aTextLen1) || (0 == aTextLen2) ||
      NS_IS_HIGH_SURROGATE(aText1[aTextLen1-1]) && 
      NS_IS_LOW_SURROGATE(aText2[0]) )  //Do not separate a surrogate pair
  {
    return PR_FALSE;
  }

  nsAutoString concat(aText1, aTextLen1);
  concat.Append(aText2, aTextLen2);

  nsAutoTArray<PRPackedBool, 2000> breakState;
  if (!breakState.AppendElements(concat.Length()))
    return NS_ERROR_OUT_OF_MEMORY;

  GetJISx4051Breaks(concat.Data(), concat.Length(), breakState.Elements());

  return breakState[aTextLen1];
}


PRInt32
nsPangoLineBreaker::Next(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos) 
{
  NS_ASSERTION(aText, "aText shouldn't be null");
  NS_ASSERTION(aLen > aPos, "Illegal value (length > position)");

  nsAutoTArray<PRPackedBool, 2000> breakState;
  if (!breakState.AppendElements(aLen))
    return NS_ERROR_OUT_OF_MEMORY;

  GetJISx4051Breaks(aText, aLen, breakState.Elements());

  while (++aPos < aLen)
    if (breakState[aPos])
      return aPos;

  return NS_LINEBREAKER_NEED_MORE_TEXT;
}


PRInt32
nsPangoLineBreaker::Prev(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos) 
{
  NS_ASSERTION(aText, "aText shouldn't be null");
  NS_ASSERTION(aLen > aPos, "Illegal value (length > position)");

  nsAutoTArray<PRPackedBool, 2000> breakState;
  if (!breakState.AppendElements(aLen))
    return NS_ERROR_OUT_OF_MEMORY;

  GetJISx4051Breaks(aText, aLen, breakState.Elements());

  while (aPos > 0)
    if (breakState[--aPos])
      return aPos;

  return NS_LINEBREAKER_NEED_MORE_TEXT;
}

void
nsPangoLineBreaker::GetJISx4051Breaks(const PRUnichar* aText, PRUint32 aLen,
                                      PRPackedBool* aBreakBefore)
{
  NS_ASSERTION(aText, "aText shouldn't be null");

  nsAutoTArray<PangoLogAttr, 2000> attrBuffer;
  if (!attrBuffer.AppendElements(aLen + 1))
    return;

  NS_ConvertUTF16toUTF8 aUTF8(aText, aLen);

  const gchar* p = aUTF8.Data();
  const gchar* end = p + aUTF8.Length();
  PRUint32     u16Offset = 0;

  static PangoLanguage* language = pango_language_from_string("en");

  while (p < end)
  {
    PangoLogAttr* attr = attrBuffer.Elements();
    pango_get_log_attrs(p, end - p, -1, language, attr, attrBuffer.Length());

    while (p < end)
    {
      aBreakBefore[u16Offset] = attr->is_line_break;
      if (NS_IS_LOW_SURROGATE(aText[u16Offset]))
        aBreakBefore[++u16Offset] = PR_FALSE; // Skip high surrogate
      ++u16Offset;

      PRUint32 ch = UTF8CharEnumerator::NextChar(&p, end);
      ++attr;

      if (ch == 0) {
        // pango_break (pango 1.16.2) only analyses text before the
        // first NUL (but sets one extra attr). Workaround loop to call
        // pango_break again to analyse after the NUL is done somewhere else
        // (gfx/thebes/src/gfxPangoFonts.cpp: SetupClusterBoundaries()).
        // So, we do the same here for pango_get_log_attrs.
        break;
      }
    }
  }
}

