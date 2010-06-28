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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef GFX_TEXT_RUN_CACHE_H
#define GFX_TEXT_RUN_CACHE_H

#include "gfxFont.h"

/**
 * A simple textrun cache for textruns that do not carry state
 * (e.g., actual or potential linebreaks) and do not need complex initialization.
 * The lifetimes of these textruns are managed by the cache (they are auto-expired
 * after a certain period of time).
 */
class THEBES_API gfxTextRunCache {
public:
    /**
     * Get a textrun for the given text, using a global cache. The textrun
     * must be released via ReleaseTextRun, not deleted.
     * Do not set any state in the textrun (e.g. actual or potential linebreaks).
     * Flags IS_8BIT and IS_ASCII are automatically set appropriately.
     * Flag IS_PERSISTENT must NOT be set unless aText is guaranteed to live
     * forever.
     * The string can contain any characters, invalid ones will be stripped
     * properly.
     */
    static gfxTextRun *MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                   gfxFontGroup *aFontGroup,
                                   gfxContext *aRefContext,
                                   PRUint32 aAppUnitsPerDevUnit,
                                   PRUint32 aFlags);

    /**
     * As above, but allows a full Parameters object to be passed in.
     */
    static gfxTextRun *MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                                   gfxFontGroup *aFontGroup,
                                   const gfxTextRunFactory::Parameters* aParams,
                                   PRUint32 aFlags);

    /**
     * Get a textrun for the given text, using a global cache. The textrun
     * must be released via ReleaseTextRun, not deleted.
     * Do not set any state in the textrun (e.g. actual or potential linebreaks).
     * Flags IS_8BIT, IS_ASCII and HAS_SURROGATES are automatically set
     * appropriately.
     * Flag IS_PERSISTENT must NOT be set unless aText is guaranteed to live
     * forever.
     * The string can contain any characters, invalid ones will be stripped
     * properly.
     */
    static gfxTextRun *MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                                   gfxFontGroup *aFontGroup,
                                   gfxContext *aRefContext,
                                   PRUint32 aAppUnitsPerDevUnit,
                                   PRUint32 aFlags);
    
    /**
     * Release a previously acquired textrun. Consider using AutoTextRun
     * instead of calling this.
     */
    static void ReleaseTextRun(gfxTextRun *aTextRun);

    class AutoTextRun {
    public:
    	AutoTextRun(gfxTextRun *aTextRun) : mTextRun(aTextRun) {}
    	AutoTextRun() : mTextRun(nsnull) {}
    	AutoTextRun& operator=(gfxTextRun *aTextRun) {
            gfxTextRunCache::ReleaseTextRun(mTextRun);
            mTextRun = aTextRun;
            return *this;
        }
        ~AutoTextRun() {
            gfxTextRunCache::ReleaseTextRun(mTextRun);
        }
        gfxTextRun *get() { return mTextRun; }
        gfxTextRun *operator->() { return mTextRun; }
    private:
        gfxTextRun *mTextRun;
    };

protected:
    friend class gfxPlatform;

    static nsresult Init();
    static void Shutdown();
};

#endif /* GFX_TEXT_RUN_CACHE_H */
