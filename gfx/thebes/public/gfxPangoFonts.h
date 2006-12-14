/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef GFX_PANGOFONTS_H
#define GFX_PANGOFONTS_H

#include "gfxTypes.h"
#include "gfxFont.h"

#include <pango/pango.h>
#include <X11/Xft/Xft.h>

#include "nsDataHashtable.h"

class gfxPangoFont : public gfxFont {
public:
    gfxPangoFont (const nsAString& aName,
                  const gfxFontStyle *aFontStyle);
    virtual ~gfxPangoFont ();

    virtual const gfxFont::Metrics& GetMetrics();

    PangoFontDescription* GetPangoFontDescription() { RealizeFont(); return mPangoFontDesc; }
    PangoContext* GetPangoContext() { RealizeFont(); return mPangoCtx; }

    void GetMozLang(nsACString &aMozLang);
    void GetActualFontFamily(nsACString &aFamily);
    PangoFont* GetPangoFont();

    XftFont * GetXftFont () { RealizeXftFont (); return mXftFont; }
    gfxFloat GetAdjustedSize() { RealizeFont(); return mAdjustedSize; }

protected:
    PangoFontDescription *mPangoFontDesc;
    PangoContext *mPangoCtx;

    XftFont * mXftFont;

    PRBool mHasMetrics;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;

    void RealizeFont(PRBool force = PR_FALSE);
    void RealizeXftFont(PRBool force = PR_FALSE);
    void GetSize(const char *aString, PRUint32 aLength, gfxSize& inkSize, gfxSize& logSize);
};

class THEBES_API gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle);
    virtual ~gfxPangoFontGroup ();

    virtual gfxTextRun *MakeTextRun(const nsAString& aString);
    virtual gfxTextRun *MakeTextRun(const nsACString& aCString);

    gfxPangoFont *GetFontAt(PRInt32 i) {
        return NS_STATIC_CAST(gfxPangoFont*, 
                              NS_STATIC_CAST(gfxFont*, mFonts[i]));
    }

    gfxPangoFont *GetCachedFont(const nsAString& aName) const {
        nsRefPtr<gfxPangoFont> font;
        if (mFontCache.Get(aName, &font))
            return font;
        return nsnull;
    }

    void PutCachedFont(const nsAString& aName, gfxPangoFont *aFont) {
        mFontCache.Put(aName, aFont);
    }
protected:
    static PRBool FontCallback (const nsAString& fontName,
                                const nsACString& genericName,
                                void *closure);
private:
    nsDataHashtable<nsStringHashKey, nsRefPtr<gfxPangoFont> > mFontCache;
    nsTArray<gfxFontStyle> mAdditionalStyles;
};

class THEBES_API gfxXftTextRun : public gfxTextRun {
public:
    gfxXftTextRun(const nsAString& aString, gfxPangoFontGroup *aFontGroup);
    gfxXftTextRun(const nsACString& aString, gfxPangoFontGroup *aFontGroup);
    ~gfxXftTextRun();

    virtual void Draw(gfxContext *aContext, gfxPoint pt);
    virtual gfxFloat Measure(gfxContext *aContext);

    virtual void SetSpacing(const nsTArray<gfxFloat>& spacingArray);
    virtual const nsTArray<gfxFloat> *const GetSpacing() const;

private:
    nsString  mWString;
    nsCString mCString;

    PRBool mIsWide;

    gfxPangoFontGroup *mGroup;

    int mWidth, mHeight;

    nsTArray<gfxFloat> mSpacing;
    nsTArray<PRInt32>  mUTF8Spacing;
};

struct TextSegment;

class THEBES_API gfxPangoTextRun : public gfxTextRun {
public:
    gfxPangoTextRun(const nsAString& aString, gfxPangoFontGroup *aFontGroup);
    ~gfxPangoTextRun();

    virtual void Draw(gfxContext *aContext, gfxPoint pt);
    virtual gfxFloat Measure(gfxContext *aContext);

    virtual void SetSpacing(const nsTArray<gfxFloat>& spacingArray);
    virtual const nsTArray<gfxFloat> *const GetSpacing() const;

private:
    nsString  mString;
    nsCString mUTF8String;

    gfxPangoFontGroup *mGroup;

    PangoLayout *mPangoLayout;
    int mWidth;

    int MeasureOrDraw(gfxContext *aContext, PRBool aDraw, gfxPoint aPt);
    int MeasureOrDrawFast(gfxContext *aContext, PRBool aDraw,
                          gfxPoint aPt, PRBool aIsRTL, gfxPangoFont *aFont);
    int MeasureOrDrawItemizing(gfxContext *aContext, PRBool aDraw,
                               gfxPoint aPt, PRBool aIsRTL,
                               gfxPangoFont *aFont);
    int DrawFromCache(gfxContext *aContext, gfxPoint aPt);

    nsTArray<gfxFloat>    mSpacing;
    nsTArray<PRInt32>     mUTF8Spacing;
    nsTArray<TextSegment> mSegments;
};

#endif /* GFX_PANGOFONTS_H */
