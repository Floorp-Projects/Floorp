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
 * The Original Code is Netscape.com code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@netscape.com>
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

#ifndef nsX11AlphaBlend_h__
#define nsX11AlphaBlend_h__

#include <X11/Xlib.h>
#include "nsColor.h"

class nsAntiAliasedGlyph;

#ifdef DEBUG
#ifndef DEBUG_SHOW_GLYPH_BOX
# define DEBUG_SHOW_GLYPH_BOX 0
#endif
void AADrawBox(XImage *, PRInt32, PRInt32, PRInt32, PRInt32, nscolor, PRUint8);
#if DEBUG_SHOW_GLYPH_BOX
# define DEBUG_AADRAWBOX(i,x,y,w,h,r,g,b,a) \
    PR_BEGIN_MACRO \
      nscolor color NS_RGB((r),(g),(b)); \
      AADrawBox((i), (x), (y), (w), (h), color, (a)); \
    PR_END_MACRO
#else
# define DEBUG_AADRAWBOX(i,x,y,w,h,r,g,b,a)
#endif
#endif

void     nsX11AlphaBlendFreeGlobals(void);
nsresult nsX11AlphaBlendInitGlobals(Display *dsp);


typedef void    (*blendGlyph)(XImage *, nsAntiAliasedGlyph *, PRUint8*,
                              nscolor, int, int);
typedef void    (*blendPixel)(XImage *, int, int, nscolor, int);
typedef nscolor (*pixelToNSColor)(unsigned long aPixel);

///////////////////////////////////////////////////////////////////////
//
// class nsX11AlphaBlend class definition
//
///////////////////////////////////////////////////////////////////////
class nsX11AlphaBlend {
public:
  inline static PRBool     CanAntiAlias()      { return sAvailable; };
  inline static blendPixel GetBlendPixel()     { return sBlendPixel; };
  inline static blendGlyph GetBlendGlyph()     { return sBlendMonoImage; };

  static XImage*  GetXImage(PRUint32 width, PRUint32 height);
  static void     FreeGlobals();
  static nsresult InitGlobals(Display *dsp);
  static XImage*  GetBackground(Display *, int, Drawable,
                                PRInt32, PRInt32, PRUint32, PRUint32);
  static nscolor  PixelToNSColor(unsigned long aPixel);

protected:
  static void ClearGlobals();
  static void ClearFunctions();
  static PRBool InitLibrary(Display *dsp);

  static PRBool         sAvailable;
  static PRUint16       sBitmapPad;
  static PRUint16       sBitsPerPixel;
  static blendGlyph     sBlendMonoImage;
  static blendPixel     sBlendPixel;
  static PRUint16       sBytesPerPixel;
  static int            sDepth;
  static PRBool         sInited;
  static pixelToNSColor sPixelToNSColor;
};

#endif /* nsX11AlphaBlend_h__ */
