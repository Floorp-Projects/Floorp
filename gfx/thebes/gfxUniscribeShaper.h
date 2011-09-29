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
 * Portions created by the Initial Developer are Copyright (C) 2005-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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

#ifndef GFX_UNISCRIBESHAPER_H
#define GFX_UNISCRIBESHAPER_H

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxGDIFont.h"

#include <usp10.h>
#include <cairo-win32.h>


class gfxUniscribeShaper : public gfxFontShaper
{
public:
    gfxUniscribeShaper(gfxGDIFont *aFont)
        : gfxFontShaper(aFont)
        , mScriptCache(NULL)
    {
        MOZ_COUNT_CTOR(gfxUniscribeShaper);
    }

    virtual ~gfxUniscribeShaper()
    {
        MOZ_COUNT_DTOR(gfxUniscribeShaper);
    }

    virtual bool InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript);

    SCRIPT_CACHE *ScriptCache() { return &mScriptCache; }

    gfxGDIFont *GetFont() { return static_cast<gfxGDIFont*>(mFont); }

private:
    SCRIPT_CACHE mScriptCache;

    enum {
        kUnicodeVS17 = gfxFontUtils::kUnicodeVS17,
        kUnicodeVS256 = gfxFontUtils::kUnicodeVS256
    };
};

#endif /* GFX_UNISCRIBESHAPER_H */
