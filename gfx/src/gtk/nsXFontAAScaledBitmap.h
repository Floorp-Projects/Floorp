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

#ifndef nsXFontAAScaledBitmap_h__
#define nsXFontAAScaledBitmap_h__

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include "nspr.h"
#include "nsXFont.h"
#include "nsAntiAliasedGlyph.h"

extern PRUint8 gAASBDarkTextMinValue;
extern double  gAASBDarkTextGain;
extern PRUint8 gAASBLightTextMinValue;
extern double  gAASBLightTextGain;


#define SCALED_SIZE(x) (PRInt32)(rint(((double)(x))*mRatio))
class nsHashtable;

class nsXFontAAScaledBitmap : public nsXFont {
public:
  // we use PRUint16 instead of PRUint32 for the final two arguments in this
  // constructor to work around a GCC 2.95[.3] bug which would otherwise cause
  // these parameters to be corrupted in the callee.  n.b. at the time of
  // writing the only caller is passing PRUint16 values anyway (and within
  // the constructor we go on toassign a parameter to a PRUint16-sized member
  // variable) so semantically nothing is lost.
  nsXFontAAScaledBitmap(Display *aDisplay, int aScreen, GdkFont *,
                        PRUint16, PRUint16);
  ~nsXFontAAScaledBitmap();

  void         DrawText8(GdkDrawable *Drawable, GdkGC *GC, PRInt32, PRInt32,
                         const char *, PRUint32);
  void         DrawText16(GdkDrawable *Drawable, GdkGC *GC, PRInt32, PRInt32,
                          const XChar2b *, PRUint32);
  PRBool       GetXFontProperty(Atom, unsigned long *);
  XFontStruct *GetXFontStruct();
  PRBool       LoadFont();
  void         TextExtents8(const char *, PRUint32, PRInt32*, PRInt32*,
                            PRInt32*, PRInt32*, PRInt32*);
  void         TextExtents16(const XChar2b *, PRUint32, PRInt32*, PRInt32*,
                             PRInt32*, PRInt32*, PRInt32*);
  PRInt32      TextWidth8(const char *, PRUint32);
  PRInt32      TextWidth16(const XChar2b *, PRUint32);
  void         UnloadFont();

public:
  static PRBool InitGlobals(Display *aDisplay, int aScreen);
  static void   FreeGlobals();

protected:
  void         DrawText8or16(GdkDrawable *Drawable, GdkGC *GC, PRInt32,
                             PRInt32, void *, PRUint32);
  void         TextExtents8or16(void *, PRUint32, PRInt32*, PRInt32*,
                             PRInt32*, PRInt32*, PRInt32*);
  PRBool GetScaledGreyImage(const char *, nsAntiAliasedGlyph **);
#ifdef DEBUG
  void dump_XImage_blue_data(XImage *ximage);
#endif
  static PRBool DisplayIsLocal(Display *);

protected:
  PRBool       mAlreadyLoaded;
  Display     *mDisplay;
  GC           mForegroundGC;
  GdkFont     *mGdkFont;
  nsHashtable* mGlyphHash;
  double       mRatio;
  XFontStruct  mScaledFontInfo;
  GlyphMetrics mScaledMax;
  int          mScreen;
  Pixmap       mUnscaledBitmap;
  XFontStruct *mUnscaledFontInfo;
  GlyphMetrics mUnscaledMax;
  PRUint16     mUnscaledSize;

// class globals
protected:
  static Display *sDisplay;
  static GC       sBackgroundGC; // used to clear the pixmaps
                                 // before drawing the glyph
  static PRUint8  sWeightedScaleDarkText[256];
  static PRUint8  sWeightedScaleLightText[256];
};

#endif /* nsXFontAAScaledBitmap_h__ */
