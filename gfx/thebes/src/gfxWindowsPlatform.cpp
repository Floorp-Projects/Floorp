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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxWindowsPlatform.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"

nsStringArray *gfxWindowsPlatform::mFontList = nsnull;

gfxWindowsPlatform::gfxWindowsPlatform()
{
    mFontList = new nsStringArray(20);

    Init();
}

gfxWindowsPlatform::~gfxWindowsPlatform()
{
    delete mFontList;
}

void
gfxWindowsPlatform::Init()
{
    LOGFONT logFont;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfFaceName[0] = 0;
    logFont.lfPitchAndFamily = 0;

    // Use the screen DC here.. should we use something else for printing?
    HDC dc = ::GetDC(nsnull);
    EnumFontFamilies(dc, nsnull, gfxWindowsPlatform::FontEnumProc, 0);
    ::ReleaseDC(nsnull, dc);

    mFontList->Sort();

    PRInt32 len = mFontList->Count();
    for (PRInt32 i = len - 1; i >= 1; --i) {
        const nsString *string1 = mFontList->StringAt(i);
        const nsString *string2 = mFontList->StringAt(i-1);
        if (string1->Equals(*string2))
            mFontList->RemoveStringAt(i);
    }
    mFontList->Compact();
}

gfxASurface*
gfxWindowsPlatform::CreateOffscreenSurface(PRUint32 width,
                                           PRUint32 height,
                                           gfxASurface::gfxImageFormat imageFormat)
{
    return new gfxWindowsSurface(nsnull, width, height, imageFormat);
}

int CALLBACK 
gfxWindowsPlatform::FontEnumProc(const LOGFONT *logFont,
                                 const TEXTMETRIC *metrics,
                                 DWORD fontType, LPARAM data)
{
    // Ignore vertical fonts
    if (logFont->lfFaceName[0] == '@') {
        return 1;
    }

    PRUnichar name[LF_FACESIZE];
    name[0] = 0;
    ::MultiByteToWideChar(CP_ACP, 0, logFont->lfFaceName,
                          strlen(logFont->lfFaceName) + 1, name,
                          sizeof(name)/sizeof(name[0]));

    mFontList->AppendString(nsDependentString(name));

    return 1;
}

nsresult
gfxWindowsPlatform::GetFontList(const nsACString& aLangGroup,
                                const nsACString& aGenericFamily,
                                nsStringArray& aListOfFonts)
{
    // XXX we need to pay attention to aLangGroup and aGenericFamily here.
    aListOfFonts.Clear();
    aListOfFonts = *mFontList;
    aListOfFonts.Sort();
    aListOfFonts.Compact();

    return NS_OK;
}

IMultiLanguage *
gfxWindowsPlatform::GetMLangService()
{
    if (!mMLang) {
        HRESULT rv;
        nsRefPtr<IMultiLanguage> multiLanguage;
        rv = CoCreateInstance(CLSID_CMultiLanguage, NULL,
                              CLSCTX_ALL, IID_IMultiLanguage, getter_AddRefs(mMLang));
        if (FAILED(rv))
            return nsnull;
    }

    return mMLang;
}
