/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfx-config.h"
#include "nscore.h"
#include "nsXFontNormal.h"
#include "nsRenderingContextGTK.h"
#include "nsGdkUtils.h"

void
nsXFontNormal::DrawText8(GdkDrawable *aDrawable, GdkGC *aGC,
                         PRInt32 aX, PRInt32 aY,
                        const char *aString, PRUint32 aLength)
{
  my_gdk_draw_text(aDrawable, mGdkFont, aGC, aX, aY, aString, aLength);
}

void
nsXFontNormal::DrawText16(GdkDrawable *aDrawable, GdkGC *aGC,
                         PRInt32 aX, PRInt32 aY,
                        const XChar2b *aString, PRUint32 aLength)
{
  my_gdk_draw_text(aDrawable, mGdkFont, aGC, aX, aY,
                   (const char *)aString, aLength*2);
}

PRBool
nsXFontNormal::GetXFontProperty(Atom aAtom, unsigned long *aValue)
{
  NS_ASSERTION(mGdkFont, "GetXFontProperty called before font loaded");
  if (mGdkFont==nsnull)
    return PR_FALSE;

  XFontStruct *fontInfo = (XFontStruct *)GDK_FONT_XFONT(mGdkFont);

  return ::XGetFontProperty(fontInfo, aAtom, aValue);
}

XFontStruct *
nsXFontNormal::GetXFontStruct()
{
  NS_ASSERTION(mGdkFont, "GetXFontStruct called before font loaded");
  if (mGdkFont==nsnull)
    return nsnull;

  return (XFontStruct *)GDK_FONT_XFONT(mGdkFont);
}

PRBool
nsXFontNormal::LoadFont()
{
  if (!mGdkFont)
    return PR_FALSE;
  XFontStruct *fontInfo = (XFontStruct *)GDK_FONT_XFONT(mGdkFont);
  mIsSingleByte = (fontInfo->min_byte1 == 0) && (fontInfo->max_byte1 == 0);
  return PR_TRUE;
}

nsXFontNormal::nsXFontNormal(GdkFont *aGdkFont)
{
  mGdkFont = ::gdk_font_ref(aGdkFont);
}

void
nsXFontNormal::TextExtents8(const char *aString, PRUint32 aLength,
                            PRInt32* aLBearing, PRInt32* aRBearing,
                            PRInt32* aWidth, PRInt32* aAscent,
                            PRInt32* aDescent)
{
  gdk_text_extents(mGdkFont, aString, aLength,
                    aLBearing, aRBearing, aWidth, aAscent, aDescent);
}

void
nsXFontNormal::TextExtents16(const XChar2b *aString, PRUint32 aLength,
                            PRInt32* aLBearing, PRInt32* aRBearing,
                            PRInt32* aWidth, PRInt32* aAscent,
                            PRInt32* aDescent)
{
  gdk_text_extents(mGdkFont, (const char *)aString, aLength*2,
                    aLBearing, aRBearing, aWidth, aAscent, aDescent);
}

PRInt32
nsXFontNormal::TextWidth8(const char *aString, PRUint32 aLength)
{
  NS_ASSERTION(mGdkFont, "TextWidth8 called before font loaded");
  if (mGdkFont==nsnull)
    return 0;
  PRInt32 width = gdk_text_width(mGdkFont, aString, aLength);
  return width;
}

PRInt32
nsXFontNormal::TextWidth16(const XChar2b *aString, PRUint32 aLength)
{
  NS_ASSERTION(mGdkFont, "TextWidth16 called before font loaded");
  if (mGdkFont==nsnull)
    return 0;
  PRInt32 width = gdk_text_width(mGdkFont, (const char *)aString, aLength*2);
  return width;
}

void
nsXFontNormal::UnloadFont()
{
  delete this;
}

nsXFontNormal::~nsXFontNormal()
{
  if (mGdkFont) {
    ::gdk_font_unref(mGdkFont);
  }
}

