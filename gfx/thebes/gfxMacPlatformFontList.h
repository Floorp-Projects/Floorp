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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef gfxMacPlatformFontList_H_
#define gfxMacPlatformFontList_H_

#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"

#include "gfxPlatformFontList.h"
#include "gfxPlatform.h"
#include "gfxPlatformMac.h"

#include <Carbon/Carbon.h>

#include "nsUnicharUtils.h"
#include "nsTArray.h"

class gfxMacPlatformFontList;

// a single member of a font family (i.e. a single face, such as Times Italic)
class MacOSFontEntry : public gfxFontEntry
{
public:
    friend class gfxMacPlatformFontList;

    virtual ~MacOSFontEntry() {
        ::CGFontRelease(mFontRef);
    }

    virtual CGFontRef GetFontRef() = 0;

    virtual nsresult GetFontTable(PRUint32 aTableTag,
                                  FallibleTArray<PRUint8>& aBuffer) = 0;

    nsresult ReadCMAP();

    bool RequiresAATLayout() const { return mRequiresAAT; }

    bool IsCFF();

protected:
    MacOSFontEntry(const nsAString& aPostscriptName, PRInt32 aWeight,
                   gfxFontFamily *aFamily, bool aIsStandardFace = false);

    virtual gfxFont* CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold);

    virtual bool HasFontTable(PRUint32 aTableTag) = 0;

    CGFontRef mFontRef; // owning reference to the CGFont, released on destruction

    bool mFontRefInitialized;
    bool mRequiresAAT;
    bool mIsCFF;
    bool mIsCFFInitialized;
};

// concrete subclasses of MacOSFontEntry: ATSFontEntry for 10.5, CGFontEntry for 10.6+
class ATSFontEntry : public MacOSFontEntry
{
public:
    ATSFontEntry(const nsAString& aPostscriptName, PRInt32 aWeight,
                 gfxFontFamily *aFamily, bool aIsStandardFace = false);

    // for use with data fonts
    ATSFontEntry(const nsAString& aPostscriptName, ATSFontRef aFontRef,
                 PRUint16 aWeight, PRUint16 aStretch, PRUint32 aItalicStyle,
                 gfxUserFontData *aUserFontData, bool aIsLocal);

    ATSFontRef GetATSFontRef();

    virtual CGFontRef GetFontRef();

    virtual nsresult GetFontTable(PRUint32 aTableTag,
                                  FallibleTArray<PRUint8>& aBuffer);

protected:
    virtual bool HasFontTable(PRUint32 aTableTag);

    ATSFontRef   mATSFontRef;
    bool mATSFontRefInitialized;
};

class CGFontEntry : public MacOSFontEntry
{
public:
    CGFontEntry(const nsAString& aPostscriptName, PRInt32 aWeight,
                gfxFontFamily *aFamily, bool aIsStandardFace = false);

    // for use with data fonts
    CGFontEntry(const nsAString& aPostscriptName, CGFontRef aFontRef,
                PRUint16 aWeight, PRUint16 aStretch, PRUint32 aItalicStyle,
                bool aIsUserFont, bool aIsLocal);

    virtual CGFontRef GetFontRef();

    virtual nsresult GetFontTable(PRUint32 aTableTag,
                                  FallibleTArray<PRUint8>& aBuffer);

protected:
    virtual bool HasFontTable(PRUint32 aTableTag);
};

class gfxMacPlatformFontList : public gfxPlatformFontList {
public:
    static gfxMacPlatformFontList* PlatformFontList() {
        return static_cast<gfxMacPlatformFontList*>(sPlatformFontList);
    }

    static PRInt32 AppleWeightToCSSWeight(PRInt32 aAppleWeight);

    virtual gfxFontEntry* GetDefaultFont(const gfxFontStyle* aStyle, bool& aNeedsBold);

    virtual bool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);
    
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData, PRUint32 aLength);

    void ClearPrefFonts() { mPrefFonts.Clear(); }

    static bool UseATSFontEntry() {
        return gfxPlatformMac::GetPlatform()->OSXVersion() < MAC_OS_X_VERSION_10_6_HEX;
    }

private:
    friend class gfxPlatformMac;

    gfxMacPlatformFontList();

    // initialize font lists
    virtual nsresult InitFontList();

    // special case font faces treated as font families (set via prefs)
    void InitSingleFaceList();

    gfxFontEntry* MakePlatformFontCG(const gfxProxyFontEntry *aProxyEntry,
                                     const PRUint8 *aFontData, PRUint32 aLength);

    gfxFontEntry* MakePlatformFontATS(const gfxProxyFontEntry *aProxyEntry,
                                      const PRUint8 *aFontData, PRUint32 aLength);

    static void ATSNotification(ATSFontNotificationInfoRef aInfo, void* aUserArg);

    // keep track of ATS generation to prevent unneeded updates when loading downloaded fonts
    PRUint32 mATSGeneration;

    enum {
        kATSGenerationInitial = -1
    };
};

#endif /* gfxMacPlatformFontList_H_ */
