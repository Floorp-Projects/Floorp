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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef NS_UNICODEPROPERTIES_H
#define NS_UNICODEPROPERTIES_H

#include "prtypes.h"
#include "nsIUGenCategory.h"

namespace mozilla {

namespace unicode {

extern nsIUGenCategory::nsUGenCategory sDetailedToGeneralCategory[];

PRUint32 GetMirroredChar(PRUint32 aCh);

PRUint8 GetCombiningClass(PRUint32 aCh);

// returns the detailed General Category in terms of HB_UNICODE_* values
PRUint8 GetGeneralCategory(PRUint32 aCh);

// returns the simplified Gen Category as defined in nsIUGenCategory
inline nsIUGenCategory::nsUGenCategory GetGenCategory(PRUint32 aCh) {
  return sDetailedToGeneralCategory[GetGeneralCategory(aCh)];
}

PRUint8 GetEastAsianWidth(PRUint32 aCh);

PRInt32 GetScriptCode(PRUint32 aCh);

PRUint32 GetScriptTagForCode(PRInt32 aScriptCode);

bool IsClusterExtender(PRUint32 aCh, PRUint8 aCategory);

inline bool IsClusterExtender(PRUint32 aCh) {
    return IsClusterExtender(aCh, GetGeneralCategory(aCh));
}

enum HSType {
    HST_NONE = 0x00,
    HST_L    = 0x01,
    HST_V    = 0x02,
    HST_T    = 0x04,
    HST_LV   = 0x03,
    HST_LVT  = 0x07
};

HSType GetHangulSyllableType(PRUint32 aCh);

enum ShapingType {
    SHAPING_DEFAULT   = 0x0001,
    SHAPING_ARABIC    = 0x0002,
    SHAPING_HEBREW    = 0x0004,
    SHAPING_HANGUL    = 0x0008,
    SHAPING_MONGOLIAN = 0x0010,
    SHAPING_INDIC     = 0x0020,
    SHAPING_THAI      = 0x0040
};

PRInt32 ScriptShapingType(PRInt32 aScriptCode);

// A simple iterator for a string of PRUnichar codepoints that advances
// by Unicode grapheme clusters
class ClusterIterator
{
public:
    ClusterIterator(const PRUnichar* aText, PRUint32 aLength)
        : mPos(aText), mLimit(aText + aLength)
#ifdef DEBUG
        , mText(aText)
#endif
    { }

    operator const PRUnichar* () const {
        return mPos;
    }

    bool AtEnd() const {
        return mPos >= mLimit;
    }

    void Next();

private:
    const PRUnichar* mPos;
    const PRUnichar* mLimit;
#ifdef DEBUG
    const PRUnichar* mText;
#endif
};

} // end namespace unicode

} // end namespace mozilla

#endif /* NS_UNICODEPROPERTIES_H */
