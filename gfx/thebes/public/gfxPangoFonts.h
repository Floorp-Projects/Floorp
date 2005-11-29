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
 * The Original Code is thebes gfx code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@mozilla.com>
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

class gfxPangoFont : public gfxFont {
public:
    gfxPangoFont (const nsAString& aName,
                  const gfxFontGroup *aFontGroup);
    virtual ~gfxPangoFont ();

    virtual const gfxFont::Metrics& GetMetrics();

    PangoFontDescription* GetPangoFontDescription() { RealizeFont(); return mPangoFontDesc; }
    PangoContext* GetPangoContext() { RealizeFont(); return mPangoCtx; }

protected:
    nsString mName;
    const gfxFontGroup *mFontGroup;
    const gfxFontStyle *mFontStyle;

    PangoFontDescription *mPangoFontDesc;
    PangoContext *mPangoCtx;

    PRBool mHasMetrics;
    Metrics mMetrics;

    void RealizeFont(PRBool force = PR_FALSE);
    void GetSize(const char *aString, PRUint32 aLength, gfxSize& inkSize, gfxSize& logSize);
};

class NS_EXPORT gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle);
    virtual ~gfxPangoFontGroup ();

    virtual gfxTextRun *MakeTextRun(const nsAString& aString);

protected:
    virtual gfxFont* MakeFont(const nsAString& aName);
};

class NS_EXPORT gfxPangoTextRun : public gfxTextRun {
    THEBES_DECL_ISUPPORTS_INHERITED
public:
    gfxPangoTextRun(const nsAString& aString, gfxPangoFontGroup *aFontGroup);
    ~gfxPangoTextRun();

    virtual void DrawString(gfxContext *aContext, gfxPoint pt);
    virtual gfxFloat MeasureString(gfxContext *aContext);

private:
    nsString mString;
    gfxPangoFontGroup *mGroup;

    PangoLayout *mPangoLayout;
    int mWidth, mHeight;

    void EnsurePangoLayout(gfxContext *aContext = nsnull);
};

#endif /* GFX_PANGOFONTS_H */
