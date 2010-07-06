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

#include "gfxTextRunCache.h"
#include "gfxTextRunWordCache.h"

#include "nsExpirationTracker.h"

enum {
    TEXTRUN_TIMEOUT_MS = 10 * 1000 // textruns expire after 3 * this period
};

/*
 * Cache textruns and expire them after a period of no use
 */
class TextRunExpiringCache : public nsExpirationTracker<gfxTextRun,3> {
public:
    TextRunExpiringCache()
        : nsExpirationTracker<gfxTextRun,3>(TEXTRUN_TIMEOUT_MS) {}
    ~TextRunExpiringCache() {
        AgeAllGenerations();
    }

    // This gets called when the timeout has expired on a gfxTextRun
    virtual void NotifyExpired(gfxTextRun *aTextRun) {
        RemoveObject(aTextRun);
        gfxTextRunWordCache::RemoveTextRun(aTextRun);
        delete aTextRun;
    }
};

static TextRunExpiringCache *gTextRunCache = nsnull;

gfxTextRun *
gfxTextRunCache::MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                             gfxFontGroup *aFontGroup,
                             const gfxTextRunFactory::Parameters* aParams,
                             PRUint32 aFlags)
{
    if (!gTextRunCache)
        return nsnull;
    return gfxTextRunWordCache::MakeTextRun(aText, aLength, aFontGroup,
        aParams, aFlags);
}

gfxTextRun *
gfxTextRunCache::MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
                             gfxFontGroup *aFontGroup,
                             gfxContext *aRefContext,
                             PRUint32 aAppUnitsPerDevUnit,
                             PRUint32 aFlags)
{
    if (!gTextRunCache)
        return nsnull;
    gfxTextRunFactory::Parameters params = {
        aRefContext, nsnull, nsnull, nsnull, 0, aAppUnitsPerDevUnit
    };
    return gfxTextRunWordCache::MakeTextRun(aText, aLength, aFontGroup,
        &params, aFlags);
}

gfxTextRun *
gfxTextRunCache::MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
                             gfxFontGroup *aFontGroup,
                             gfxContext *aRefContext,
                             PRUint32 aAppUnitsPerDevUnit,
                             PRUint32 aFlags)
{
    if (!gTextRunCache)
        return nsnull;
    gfxTextRunFactory::Parameters params = {
        aRefContext, nsnull, nsnull, nsnull, 0, aAppUnitsPerDevUnit
    };
    return gfxTextRunWordCache::MakeTextRun(aText, aLength, aFontGroup,
        &params, aFlags);
}

void
gfxTextRunCache::ReleaseTextRun(gfxTextRun *aTextRun)
{
    if (!aTextRun)
        return;
    if (!(aTextRun->GetFlags() & gfxTextRunWordCache::TEXT_IN_CACHE)) {
        delete aTextRun;
        return;
    }
    nsresult rv = gTextRunCache->AddObject(aTextRun);
    if (NS_FAILED(rv)) {
        delete aTextRun;
        return;
    }
}

nsresult
gfxTextRunCache::Init()
{
    gTextRunCache = new TextRunExpiringCache();
    return gTextRunCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxTextRunCache::Shutdown()
{
    delete gTextRunCache;
    gTextRunCache = nsnull;
}

