/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2002 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsFontConfigUtils.h"

struct MozXftLangGroup {
    const char    *mozLangGroup;
    FcChar32       character;
    const FcChar8 *XftLang;
};

const MozXftLangGroup MozXftLangGroups[] = {
    { "x-western",      0x0041, (const FcChar8 *)"en" },
    { "x-central-euro", 0x0100, (const FcChar8 *)"pl" },
    { "x-cyrillic",     0x0411, (const FcChar8 *)"ru" },
    { "x-baltic",       0x0104, (const FcChar8 *)"lv" },
    { "x-devanagari",   0x0905, (const FcChar8 *)"hi" },
    { "x-tamil",        0x0B85, (const FcChar8 *)"ta" },
    { "x-unicode",      0x0000,                  0    },
    { "x-user-def",     0x0000,                  0    },
};

#define NUM_XFT_LANG_GROUPS (sizeof (MozXftLangGroups) / \
                             sizeof (MozXftLangGroups[0]))

static 
void FFREToFamily(nsACString &aFFREName, nsACString &oFamily);

static
const MozXftLangGroup*
FindFCLangGroup (nsACString &aLangGroup)
{
    for (unsigned int i=0; i < NUM_XFT_LANG_GROUPS; ++i) {
        if (aLangGroup.Equals(MozXftLangGroups[i].mozLangGroup,
                              nsCaseInsensitiveCStringComparator())) {
            return &MozXftLangGroups[i];
        }
    }

    return nsnull;
}

int
NS_CalculateSlant(PRUint8 aStyle)
{
    int fcSlant;

    switch(aStyle) {
    case NS_FONT_STYLE_ITALIC:
        fcSlant = FC_SLANT_ITALIC;
        break;
    case NS_FONT_STYLE_OBLIQUE:
        fcSlant = FC_SLANT_OBLIQUE;
        break;
    default:
        fcSlant = FC_SLANT_ROMAN;
        break;
    }

    return fcSlant;
}

int
NS_CalculateWeight (PRUint16 aWeight)
{
    /*
     * weights come in two parts crammed into one
     * integer -- the "base" weight is weight / 100,
     * the rest of the value is the "offset" from that
     * weight -- the number of steps to move to adjust
     * the weight in the list of supported font weights,
     * this value can be negative or positive.
     */
    PRInt32 baseWeight = (aWeight + 50) / 100;
    PRInt32 offset = aWeight - baseWeight * 100;

    /* clip weights to range 0 to 9 */
    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    /* Map from weight value to fcWeights index */
    static int fcWeightLookup[10] = {
        0, 0, 0, 0, 1, 1, 2, 3, 3, 4,
    };

    PRInt32 fcWeight = fcWeightLookup[baseWeight];

    /*
     * adjust by the offset value, make sure we stay inside the
     * fcWeights table
     */
    fcWeight += offset;
    if (fcWeight < 0)
        fcWeight = 0;
    if (fcWeight > 4)
        fcWeight = 4;

    /* Map to final FC_WEIGHT value */
    static int fcWeights[5] = {
        FC_WEIGHT_LIGHT,      /* 0 */
        FC_WEIGHT_MEDIUM,     /* 1 */
        FC_WEIGHT_DEMIBOLD,   /* 2 */
        FC_WEIGHT_BOLD,       /* 3 */
        FC_WEIGHT_BLACK,      /* 4 */
    };

    return fcWeights[fcWeight];
}

void
NS_AddLangGroup(FcPattern *aPattern, nsIAtom *aLangGroup)
{
    // Find the FC lang group for this lang group
    nsCAutoString cname;
    aLangGroup->ToUTF8String(cname);

    // see if the lang group needs to be translated from mozilla's
    // internal mapping into fontconfig's
    const struct MozXftLangGroup *langGroup;
    langGroup = FindFCLangGroup(cname);

    // if there's no lang group, just use the lang group as it was
    // passed to us
    //
    // we're casting away the const here for the strings - should be
    // safe.
    if (!langGroup)
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)cname.get());
    else if (langGroup->XftLang)
        FcPatternAddString(aPattern, FC_LANG, (FcChar8 *)langGroup->XftLang);
}

void
NS_AddFFRE(FcPattern *aPattern, nsCString *aFamily, PRBool aWeak)
{
    nsCAutoString family;
    FFREToFamily(*aFamily, family);

    FcValue v;
    v.type = FcTypeString;
    // casting away the const here, should be safe
    v.u.s = (FcChar8 *)family.get();

    if (aWeak)
        FcPatternAddWeak(aPattern, FC_FAMILY, v, FcTrue);
    else
        FcPatternAdd(aPattern, FC_FAMILY, v, FcTrue);
}

/* static */
void
FFREToFamily(nsACString &aFFREName, nsACString &oFamily)
{
  if (NS_FFRECountHyphens(aFFREName) == 3) {
      PRInt32 familyHyphen = aFFREName.FindChar('-') + 1;
      PRInt32 registryHyphen = aFFREName.FindChar('-',familyHyphen);
      oFamily.Append(Substring(aFFREName, familyHyphen,
                               registryHyphen-familyHyphen));
  }
  else {
      oFamily.Append(aFFREName);
  }
}

int
NS_FFRECountHyphens (nsACString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

PRBool
NS_IsASCIIFontName(const nsString& aName)
{
    PRUint32 len = aName.Length();
    const PRUnichar* str = aName.get();
    for (PRUint32 i = 0; i < len; i++) {
        /*
         * X font names are printable ASCII, ignore others (for now)
         */
        if ((str[i] < 0x20) || (str[i] > 0x7E)) {
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}
