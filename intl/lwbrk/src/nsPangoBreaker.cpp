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

#include "nsComplexBreaker.h"

#include <pango/pango-break.h>
#include "nsUTF8Utils.h"
#include "nsString.h"
#include "nsTArray.h"

void
NS_GetComplexLineBreaks(const PRUnichar* aText, PRUint32 aLength,
                        PRUint8* aBreakBefore)
{
  NS_ASSERTION(aText, "aText shouldn't be null");

  memset(aBreakBefore, PR_FALSE, aLength * sizeof(PRUint8));

  nsAutoTArray<PangoLogAttr, 2000> attrBuffer;
  if (!attrBuffer.AppendElements(aLength + 1))
    return;

  NS_ConvertUTF16toUTF8 aUTF8(aText, aLength);

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

      bool err;
      PRUint32 ch = UTF8CharEnumerator::NextChar(&p, end, &err);
      ++attr;

      if (ch == 0 || err) {
        // pango_break (pango 1.16.2) only analyses text before the
        // first NUL (but sets one extra attr). Workaround loop to call
        // pango_break again to analyse after the NUL is done somewhere else
        // (gfx/thebes/gfxPangoFonts.cpp: SetupClusterBoundaries()).
        // So, we do the same here for pango_get_log_attrs.
        break;
      }
    }
  }
}

