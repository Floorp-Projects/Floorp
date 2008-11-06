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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Daggett <jdaggett@mozilla.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "gfxUserFontSet.h"
#include "gfxPlatform.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "prlong.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *gUserFontsLog = PR_NewLogModule("userfonts");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gUserFontsLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gUserFontsLog, PR_LOG_DEBUG)

static PRUint64 sFontSetGeneration = LL_INIT(0, 0);

// TODO: support for unicode ranges not yet implemented

gfxProxyFontEntry::gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, 
             PRUint32 aWeight, 
             PRUint32 aStretch, 
             PRUint32 aItalicStyle, 
             gfxSparseBitSet *aUnicodeRanges)
    : gfxFontEntry(NS_LITERAL_STRING("Proxy")), mIsLoading(PR_FALSE)
{
    mIsProxy = PR_TRUE;
    mSrcList = aFontFaceSrcList;
    mSrcIndex = 0;
    mWeight = aWeight;
    mStretch = aStretch;
    mItalic = (aItalicStyle & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;
}

gfxProxyFontEntry::~gfxProxyFontEntry()
{

}


PRBool
gfxMixedFontFamily::FindWeightsForStyle(gfxFontEntry* aFontsForWeights[], 
                                        const gfxFontStyle& aFontStyle)
{
    PRBool italic = (aFontStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;
    PRBool matchesSomething;

    for (PRUint32 j = 0; j < 2; j++) {
        matchesSomething = PR_FALSE;
        PRUint32 numFonts = mAvailableFonts.Length();
        // build up an array of weights that match the italicness we're looking for
        for (PRUint32 i = 0; i < numFonts; i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            PRUint32 weight = fe->mWeight/100;
            if (fe->mItalic == italic) {
                aFontsForWeights[weight] = fe;
                matchesSomething = PR_TRUE;
            }
        }
        if (matchesSomething)
            break;
        italic = !italic;
    }

    return matchesSomething;
}


gfxUserFontSet::gfxUserFontSet(LoaderContext *aContext)
    : mLoaderContext(aContext)
{
    NS_ASSERTION(mLoaderContext, "font set loader context not initialized");
    mFontFamilies.Init(5);
    mLoaderContext->mUserFontSet = this;

    IncrementGeneration();
}

gfxUserFontSet::~gfxUserFontSet()
{

}

void
gfxUserFontSet::AddFontFace(const nsAString& aFamilyName, 
                            const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, 
                            PRUint32 aWeight, 
                            PRUint32 aStretch, 
                            PRUint32 aItalicStyle, 
                            gfxSparseBitSet *aUnicodeRanges)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    PRBool found;

    if (aWeight == 0)
        aWeight = FONT_WEIGHT_NORMAL;

    // stretch, italic/oblique ==> zero implies normal

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);
    if (!family) {
        family = new gfxMixedFontFamily(aFamilyName);
        mFontFamilies.Put(key, family);
    }

    // construct a new face and add it into the family
    if (family) {
        gfxProxyFontEntry *proxyEntry = 
            new gfxProxyFontEntry(aFontFaceSrcList, aWeight, aStretch, 
                                                  aItalicStyle, aUnicodeRanges);
        family->AddFontEntry(proxyEntry);
        proxyEntry->mFamily = family;
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            LOG(("userfonts (%p) added (%s) with style: %s weight: %d stretch: %d", 
                 this, NS_ConvertUTF16toUTF8(aFamilyName).get(), 
                 (aItalicStyle & FONT_STYLE_ITALIC ? "italic" : 
                     (aItalicStyle & FONT_STYLE_OBLIQUE ? "oblique" : "normal")), 
                 aWeight, aStretch));
        }
#endif
    }
}

gfxFontEntry*
gfxUserFontSet::FindFontEntry(const nsAString& aName, 
                              const gfxFontStyle& aFontStyle, 
                              PRBool& aNeedsBold)
{
    nsAutoString key(aName);
    ToLowerCase(key);

    PRBool found;

    gfxMixedFontFamily *family = mFontFamilies.GetWeak(key, &found);

    // no user font defined for this name
    if (!family)
        return nsnull;

    gfxFontEntry* fe = family->FindFontForStyle(aFontStyle, aNeedsBold);

    // if not a proxy, font has already been loaded
    if (!fe->mIsProxy)
        return fe;

    gfxProxyFontEntry *proxyEntry = static_cast<gfxProxyFontEntry*> (fe);

    // if currently loading, return null for now
    if (proxyEntry->mIsLoading)
        return nsnull;

    // hasn't been loaded yet, start the load process
    LoadStatus status;

    status = LoadNext(proxyEntry);

    // if the load succeeded immediately, the font entry was replaced so
    // search again
    if (status == STATUS_LOADED)
        return family->FindFontForStyle(aFontStyle, aNeedsBold);

    // if either loading or an error occurred, return null
    return nsnull;
}


PRBool 
gfxUserFontSet::OnLoadComplete(gfxFontEntry *aFontToLoad, 
                               const PRUint8 *aFontData, PRUint32 aLength, 
                               nsresult aDownloadStatus)
{
    NS_ASSERTION(aFontToLoad->mIsProxy, "trying to load font data for wrong font entry type");

    if (!aFontToLoad->mIsProxy)
        return PR_FALSE;

    gfxProxyFontEntry *pe = static_cast<gfxProxyFontEntry*> (aFontToLoad);

    // download successful, make platform font using font data
    if (NS_SUCCEEDED(aDownloadStatus)) {
        gfxFontEntry *fe = 
           gfxPlatform::GetPlatform()->MakePlatformFont(pe, aFontData, aLength);
        if (fe) {
            pe->mFamily->ReplaceFontEntry(pe, fe);
            IncrementGeneration();
#ifdef PR_LOGGING
            if (LOG_ENABLED()) {
                nsCAutoString fontURI;
                pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);

                LOG(("userfonts (%p) [src %d] loaded uri: (%s) for (%s) gen: %8.8x\n", 
                     this, pe->mSrcIndex, fontURI.get(), 
                     NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get(), 
                     PRUint32(mGeneration)));
            }
#endif
            return PR_TRUE;
        } else {
#ifdef PR_LOGGING
            if (LOG_ENABLED()) {
                nsCAutoString fontURI;
                pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);
                LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) error making platform font\n", 
                     this, pe->mSrcIndex, fontURI.get(), 
                     NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get()));
            }
#endif
        }
    } else {
        // download failed
#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            nsCAutoString fontURI;
            pe->mSrcList[pe->mSrcIndex].mURI->GetSpec(fontURI);
            LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) error %8.8x downloading font data\n", 
                 this, pe->mSrcIndex, fontURI.get(), 
                 NS_ConvertUTF16toUTF8(pe->mFamily->Name()).get(), 
                 aDownloadStatus));
        }
#endif
    }

    // error occurred, load next src
    LoadStatus status;

    status = LoadNext(pe);
    if (status == STATUS_LOADED)
        return PR_TRUE;

    return PR_FALSE;
}


gfxUserFontSet::LoadStatus
gfxUserFontSet::LoadNext(gfxProxyFontEntry *aProxyEntry)
{
    PRUint32 numSrc = aProxyEntry->mSrcList.Length();

    NS_ASSERTION(aProxyEntry->mSrcIndex < numSrc, "already at the end of the src list for user font");

    NS_ASSERTION(mLoaderContext, "user font loader context not initialized");
    if (!mLoaderContext)
        return STATUS_ERROR; 

    if (aProxyEntry->mIsLoading) {
        aProxyEntry->mSrcIndex++;
    } else {
        aProxyEntry->mIsLoading = PR_TRUE;
    }

    // load each src entry in turn, until a local face is found or a download begins successfully
    while (aProxyEntry->mSrcIndex < numSrc) {
        const gfxFontFaceSrc& currSrc = aProxyEntry->mSrcList[aProxyEntry->mSrcIndex];

        // src local ==> lookup and load   

        if (currSrc.mIsLocal) {
            gfxFontEntry *fe = gfxPlatform::GetPlatform()->LookupLocalFont(currSrc.mLocalName);
            if (fe) {
                aProxyEntry->mFamily->ReplaceFontEntry(aProxyEntry, fe);
                IncrementGeneration();
                LOG(("userfonts (%p) [src %d] loaded local: (%s) for (%s) gen: %8.8x\n", 
                     this, aProxyEntry->mSrcIndex, 
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(), 
                     NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get(), 
                     PRUint32(mGeneration)));
                return STATUS_LOADED;
            } else {
                LOG(("userfonts (%p) [src %d] failed local: (%s) for (%s)\n", 
                     this, aProxyEntry->mSrcIndex, 
                     NS_ConvertUTF16toUTF8(currSrc.mLocalName).get(), 
                     NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));            
            }
        } 

        // src url ==> start the load process
        else {
            if (gfxPlatform::GetPlatform()->IsFontFormatSupported(currSrc.mURI, 
                    currSrc.mFormatFlags)) {
                nsresult rv = mLoaderContext->mLoaderProc(aProxyEntry, 
                                                          currSrc.mURI,
                                                          currSrc.mReferrer,
                                                          mLoaderContext);
                PRBool loadOK = NS_SUCCEEDED(rv);
                
                if (loadOK) {
#ifdef PR_LOGGING
                    if (LOG_ENABLED()) {
                        nsCAutoString fontURI;
                        currSrc.mURI->GetSpec(fontURI);
                        LOG(("userfonts (%p) [src %d] loading uri: (%s) for (%s)\n", 
                             this, aProxyEntry->mSrcIndex, fontURI.get(), 
                             NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                    }
#endif
                    return STATUS_LOADING;                  
                } else {
#ifdef PR_LOGGING
                    if (LOG_ENABLED()) {
                        nsCAutoString fontURI;
                        currSrc.mURI->GetSpec(fontURI);
                        LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) download failed\n", 
                             this, aProxyEntry->mSrcIndex, fontURI.get(), 
                             NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                    }
#endif
                }
            } else {
#ifdef PR_LOGGING
                if (LOG_ENABLED()) {
                    nsCAutoString fontURI;
                    currSrc.mURI->GetSpec(fontURI);
                    LOG(("userfonts (%p) [src %d] failed uri: (%s) for (%s) format not supported\n", 
                         this, aProxyEntry->mSrcIndex, fontURI.get(), 
                         NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));
                }
#endif
            }
        }

        aProxyEntry->mSrcIndex++;
    }

    // all src's failed, remove this face
    LOG(("userfonts (%p) failed all src for (%s)\n", 
               this, NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get()));            

    gfxMixedFontFamily *family = aProxyEntry->mFamily;

    aProxyEntry->mFamily->RemoveFontEntry(aProxyEntry);

    // no more faces?  remove the entire family
    if (family->mAvailableFonts.Length() == 0) {
        LOG(("userfonts (%p) failed all faces, remove family (%s)\n", 
             this, NS_ConvertUTF16toUTF8(family->Name()).get()));            
        RemoveFamily(family->Name());
    }

    return STATUS_END_OF_LIST;
}


void
gfxUserFontSet::IncrementGeneration()
{
    // add one, increment again if zero
    LL_ADD(sFontSetGeneration, sFontSetGeneration, 1);
    if (LL_IS_ZERO(sFontSetGeneration))
        LL_ADD(sFontSetGeneration, sFontSetGeneration, 1);
    mGeneration = sFontSetGeneration;
}


void 
gfxUserFontSet::RemoveFamily(const nsAString& aFamilyName)
{
    nsAutoString key(aFamilyName);
    ToLowerCase(key);

    mFontFamilies.Remove(key);
}
