/* vim: set sw=4 sts=4 et cin: */
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  IBM Corp.  (code inherited from nsFontMetricsOS2 and OS2Uni)
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

#ifndef GFX_OS2_FONTS_H
#define GFX_OS2_FONTS_H

#include "gfxTypes.h"
#include "gfxFont.h"

#define INCL_GPI
#include <os2.h>
#include <cairo-os2.h>

#include "nsAutoBuffer.h"
#include "nsICharsetConverterManager.h"

class gfxOS2Font : public gfxFont {

public:
    gfxOS2Font(const nsAString &aName, const gfxFontStyle *aFontStyle);
    virtual ~gfxOS2Font();

    virtual const gfxFont::Metrics& GetMetrics();
    double GetWidth(HPS aPS, const char* aString, PRUint32 aLength);
    double GetWidth(HPS aPS, const PRUnichar* aString, PRUint32 aLength);

protected:
    //cairo_font_face_t *CairoFontFace();
    //cairo_scaled_font_t *CairoScaledFont();

private:
    void ComputeMetrics();

    //cairo_font_face_t *mFontFace;
    gfxFont::Metrics *mMetrics;
    //FATTRS mFAttrs;
};


class THEBES_API gfxOS2FontGroup : public gfxFontGroup {

public:
    gfxOS2FontGroup(const nsAString& aFamilies, const gfxFontStyle* aStyle);
    virtual ~gfxOS2FontGroup();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle) {
        NS_ERROR("NOT IMPLEMENTED");
        return nsnull;
    }
    virtual gfxTextRun *MakeTextRun(const PRUnichar* aString, PRUint32 aLength, Parameters* aParams);
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength, Parameters* aParams);

    const nsACString& GetGenericFamily() const {
        return mGenericFamily;
    }

    PRUint32 FontListLength() const {
        return mFonts.Length();
    }

    gfxOS2Font *GetFontAt(PRInt32 i) {
        return NS_STATIC_CAST(gfxOS2Font*, NS_STATIC_CAST(gfxFont*, mFonts[i]));
    }

    void AppendFont(gfxOS2Font *aFont) {
        mFonts.AppendElement(aFont);
    }

    PRBool HasFontNamed(const nsAString& aName) const {
        PRUint32 len = mFonts.Length();
        for (PRUint32 i = 0; i < len; ++i)
            if (aName.Equals(mFonts[i]->GetName()))
                return PR_TRUE;
        return PR_FALSE;
    }

protected:
    static PRBool MakeFont(const nsAString& fontName,
                           const nsACString& genericName,
                           void *closure);

private:
    friend class gfxOS2TextRun;

    nsCString mGenericFamily;
};


class THEBES_API gfxOS2TextRun {

public:
    gfxOS2TextRun(const nsAString& aString, gfxOS2FontGroup *aFontGroup);
    gfxOS2TextRun(const nsACString& aString, gfxOS2FontGroup *aFontGroup);
    ~gfxOS2TextRun();

    virtual void Draw(gfxContext *aContext, gfxPoint pt);
    virtual gfxFloat Measure(gfxContext *aContext);
    virtual void SetSpacing(const nsTArray<gfxFloat>& spacingArray);
    virtual const nsTArray<gfxFloat> *const GetSpacing() const;
    void SetRightToLeft(PRBool aIsRTL) { mIsRTL = aIsRTL; }
    PRBool IsRightToLeft() { return mIsRTL; }

protected:
private:
    /* these only return a length measurement if aDraw is PR_FALSE *
     * we have an easy and a sophisticated variant                 */
    double MeasureOrDrawFast(gfxContext *aContext, PRBool aDraw, gfxPoint aPt);
    double MeasureOrDrawSlow(gfxContext *aContext, PRBool aDraw, gfxPoint aPt);

    gfxOS2FontGroup *mGroup;

    nsString mString;
    nsCString mCString;

    const PRPackedBool mIsASCII;
    PRPackedBool mIsRTL;
    nsTArray<gfxFloat> mSpacing;
    double mLength; /* cached */
};

// =============================================================================
// class gfxOS2Uni to handle Unicode on OS/2 (copied from gfx/src/os2)
enum ConverterRequest {
    eConv_Encoder,
    eConv_Decoder
};

#define CHAR_BUFFER_SIZE 1024
typedef nsAutoBuffer<char, CHAR_BUFFER_SIZE> nsAutoCharBuffer;
typedef nsAutoBuffer<PRUnichar, CHAR_BUFFER_SIZE> nsAutoChar16Buffer;

class gfxOS2Uni {
public:
    static nsISupports* GetUconvObject(int CodePage, ConverterRequest aReq);
    static void FreeUconvObjects();
private:
    static nsICharsetConverterManager* gCharsetManager;
};

nsresult WideCharToMultiByte(int aCodePage,
                             const PRUnichar* aSrc, PRInt32 aSrcLength,
                             nsAutoCharBuffer& aResult, PRInt32& aResultLength);
nsresult MultiByteToWideChar(int aCodePage,
                             const char* aSrc, PRInt32 aSrcLength,
                             nsAutoChar16Buffer& aResult, PRInt32& aResultLength);

// =============================================================================

#endif /* GFX_OS2_FONTS_H */
