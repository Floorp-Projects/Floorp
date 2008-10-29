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

#ifndef GFX_USER_FONT_SET_H
#define GFX_USER_FONT_SET_H

#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxFontUtils.h"
#include "nsRefPtrHashtable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIFile.h"

class nsIURI;
class gfxMixedFontFamily;

// parsed CSS @font-face rule information
// lifetime: from when @font-face rule processed until font is loaded
struct gfxFontFaceSrc {
    PRPackedBool           mIsLocal;       // url or local
    nsString               mLocalName;     // full font name if local
    nsCOMPtr<nsIURI>       mURI;           // uri if url 

    // format hint flags, union of all possible formats
    // (e.g. TrueType, EOT, SVG, etc.)
    // see FLAG_FORMAT_* enum values below
    PRUint32               mFormatFlags;
};

// subclassed by loader code to contain needed context info
// lifetime: user font set lifetime 
class gfxFontLoaderContext {
public:
  gfxFontLoaderContext() { }
  virtual ~gfxFontLoaderContext() { }
};

// subclassed to store platform-specific code cleaned out when font entry is deleted
// lifetime: from when platform font is created until it is deactivated 
class gfxUserFontData {
public:
    gfxUserFontData() { }
    virtual ~gfxUserFontData() { }
};

// initially contains a set of proxy font entry objects, replaced with
// platform/user fonts as downloaded

class gfxMixedFontFamily : public gfxFontFamily {

public:
    gfxMixedFontFamily(const nsAString& aName)
        : gfxFontFamily(aName)
    { }

    virtual ~gfxMixedFontFamily() { }

    void AddFontEntry(gfxFontEntry *aFontEntry) {
        nsRefPtr<gfxFontEntry> fe = aFontEntry;
        mAvailableFonts.AppendElement(fe);
    }

    void ReplaceFontEntry(gfxFontEntry *aOldFontEntry, gfxFontEntry *aNewFontEntry) 
    {
        PRUint32 numFonts = mAvailableFonts.Length();
        for (PRUint32 i = 0; i < numFonts; i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            if (fe == aOldFontEntry) {
                mAvailableFonts[i] = aNewFontEntry;
                return;
            }
        }
    }

    void RemoveFontEntry(gfxFontEntry *aFontEntry) 
    {
        PRUint32 numFonts = mAvailableFonts.Length();
        for (PRUint32 i = 0; i < numFonts; i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            if (fe == aFontEntry) {
                mAvailableFonts.RemoveElementAt(i);
                return;
            }
        }
    }

    // temp method to determine if all proxies are loaded
    PRBool AllLoaded() 
    {
        PRUint32 numFonts = mAvailableFonts.Length();
        for (PRUint32 i = 0; i < numFonts; i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            if (fe->mIsProxy)
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    nsTArray<nsRefPtr<gfxFontEntry> >  mAvailableFonts;

protected:
    PRBool FindWeightsForStyle(gfxFontEntry* aFontsForWeights[], 
                               const gfxFontStyle& aFontStyle);

};

class gfxProxyFontEntry;

class THEBES_API gfxUserFontSet {

public:
    class LoaderContext;
    typedef PRBool (*LoaderCallback) (gfxFontEntry *aFontToLoad, nsIURI *aSrcURL, 
                                      LoaderContext *aContextData);

    class LoaderContext {
    public:
        LoaderContext(LoaderCallback aLoader)
            : mUserFontSet(nsnull), mLoaderProc(aLoader) { }
        virtual ~LoaderContext() { }

        gfxUserFontSet* mUserFontSet;
        LoaderCallback  mLoaderProc;
    };

    THEBES_INLINE_DECL_REFCOUNTING(gfxUserFontSet)

    gfxUserFontSet(LoaderContext *aContext);
    virtual ~gfxUserFontSet();

    enum {
        // no flags ==> unknown
        FLAG_FORMAT_OPENTYPE       = 1,
        FLAG_FORMAT_TRUETYPE       = 2,
        FLAG_FORMAT_TRUETYPE_AAT   = 4,
        FLAG_FORMAT_EOT            = 8,
        FLAG_FORMAT_SVG            = 16
    };

    enum LoadStatus {
        STATUS_LOADING = 0,
        STATUS_LOADED,
        STATUS_FORMAT_NOT_SUPPORTED,
        STATUS_ERROR,
        STATUS_END_OF_LIST
    };


    // add in a font face
    // weight, stretch - 0 == unknown, [1, 9] otherwise
    // italic style = constants in gfxFont.h (e.g. FONT_STYLE_NORMAL)
    // TODO: support for unicode ranges not yet implemented
    void AddFontFace(const nsAString& aFamilyName, 
                     const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, 
                     PRUint32 aWeight = 0, 
                     PRUint32 aStretch = 0, 
                     PRUint32 aItalicStyle = 0, 
                     gfxSparseBitSet *aUnicodeRanges = nsnull);

    // lookup a font entry for a given style, returns null if not loaded
    gfxFontEntry *FindFontEntry(const nsAString& aName, 
                                const gfxFontStyle& aFontStyle, PRBool& aNeedsBold);

    // when download has been completed, pass back data here
    // aDownloadStatus == NS_OK ==> download succeeded, error otherwise
    // returns true if platform font creation sucessful (or local()
    // reference was next in line)
    PRBool OnLoadComplete(gfxFontEntry *aFontToLoad, 
                          const PRUint8 *aFontData, PRUint32 aLength,
                          nsresult aDownloadStatus);

    // generation - each time a face is loaded, generation is
    // incremented so that the change can be recognized 
    PRUint64 GetGeneration() { return mGeneration; }

protected:
    // for a given proxy font entry, attempt to load the next resource
    // in the src list
    LoadStatus LoadNext(gfxProxyFontEntry *aProxyEntry);

    // increment the generation on font load
    void IncrementGeneration();

    // remove family
    void RemoveFamily(const nsAString& aFamilyName);

    // font families defined by @font-face rules
    nsRefPtrHashtable<nsStringHashKey, gfxMixedFontFamily> mFontFamilies;

    PRUint64        mGeneration;

    // owned by user font set obj, deleted within destructor
    nsAutoPtr<LoaderContext> mLoaderContext;
};

// acts a placeholder until the real font is downloaded

class gfxProxyFontEntry : public gfxFontEntry {

public:
    gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, 
                      PRUint32 aWeight, 
                      PRUint32 aStretch, 
                      PRUint32 aItalicStyle, 
                      gfxSparseBitSet *aUnicodeRanges);

    virtual ~gfxProxyFontEntry();

    PRPackedBool                           mIsLoading;
    nsTArray<gfxFontFaceSrc>               mSrcList;
    PRUint32                               mSrcIndex; // index of loading src item
    gfxMixedFontFamily*                    mFamily;
};


#endif /* GFX_USER_FONT_SET_H */
