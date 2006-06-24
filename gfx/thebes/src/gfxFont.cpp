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
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"
#include "nsReadableUtils.h"

#include "gfxFont.h"

#include "prtypes.h"
#include "gfxTypes.h"

#include "nsCRT.h"

THEBES_IMPL_REFCOUNTING(gfxFont)
THEBES_IMPL_REFCOUNTING(gfxTextRun)

gfxFont::gfxFont(const nsAString &aName, const gfxFontStyle *aFontStyle) :
    mName(aName), mStyle(aFontStyle)
{

}


gfxFontGroup::gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle)
    : mFamilies(aFamilies), mStyle(*aStyle)
{

}

PRBool
gfxFontGroup::ForEachFont(FontCreationCallback fc,
                          void *closure)
{
    return ForEachFont(mFamilies, mStyle.langGroup, fc, closure);
}

PRBool
gfxFontGroup::ForEachFont(const nsAString& aFamilies,
                          const nsACString& aLangGroup,
                          FontCreationCallback fc,
                          void *closure)
{
    const PRUnichar kSingleQuote  = PRUnichar('\'');
    const PRUnichar kDoubleQuote  = PRUnichar('\"');
    const PRUnichar kComma        = PRUnichar(',');

    nsCOMPtr<nsIPref> prefs;
    prefs = do_GetService(NS_PREF_CONTRACTID);

    nsPromiseFlatString families(aFamilies);
    const PRUnichar *p, *p_end;
    families.BeginReading(p);
    families.EndReading(p_end);
    nsAutoString family;
    nsAutoString genericFamily;

    while (p < p_end) {
        while (nsCRT::IsAsciiSpace(*p))
            if (++p == p_end)
                return PR_TRUE;

        PRBool generic;
        if (*p == kSingleQuote || *p == kDoubleQuote) {
            // quoted font family
            PRUnichar quoteMark = *p;
            if (++p == p_end)
                return PR_TRUE;
            const PRUnichar *nameStart = p;

            // XXX What about CSS character escapes?
            while (*p != quoteMark)
                if (++p == p_end)
                    return PR_TRUE;

            family = Substring(nameStart, p);
            genericFamily.SetIsVoid(PR_TRUE);

            while (++p != p_end && *p != kComma)
                /* nothing */ ;

        } else {
            // unquoted font family
            const PRUnichar *nameStart = p;
            while (++p != p_end && *p != kComma)
                /* nothing */ ;

            family = Substring(nameStart, p);
            family.CompressWhitespace(PR_FALSE, PR_TRUE);

            if (family.LowerCaseEqualsLiteral("serif") ||
                family.LowerCaseEqualsLiteral("sans-serif") ||
                family.LowerCaseEqualsLiteral("monospace") ||
                family.LowerCaseEqualsLiteral("cursive") ||
                family.LowerCaseEqualsLiteral("fantasy"))
            {
                generic = PR_TRUE;

                nsCAutoString lcFamily;
                ToLowerCase(NS_LossyConvertUTF16toASCII(family), lcFamily);

                nsCAutoString prefName("font.name.");
                prefName.Append(lcFamily);
                prefName.AppendLiteral(".");
                prefName.Append(aLangGroup);

                // prefs file always uses (must use) UTF-8 so that we can use
                // |GetCharPref| and treat the result as a UTF-8 string.
                nsXPIDLString value;
                nsresult rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(value));
                if (NS_SUCCEEDED(rv)) {
                    CopyASCIItoUTF16(lcFamily, genericFamily);
                    family = value;
                }
            } else {
                generic = PR_FALSE;
                genericFamily.SetIsVoid(PR_TRUE);
            }
        }
        
        if (!family.IsEmpty()) {
            if (!((*fc) (family, NS_LossyConvertUTF16toASCII(genericFamily), closure)))
                return PR_FALSE;
        }

        ++p; // may advance past p_end
    }

    return PR_TRUE;
}

void
gfxFontGroup::FindGenericFontFromStyle(FontCreationCallback fc,
                                       void *closure)
{
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
        return;

    nsCAutoString prefName;
    nsXPIDLString genericName;
    nsXPIDLString familyName;

    // add the default font to the end of the list
    prefName.AssignLiteral("font.default.");
    prefName.Append(mStyle.langGroup);
    nsresult rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(genericName));
    if (NS_SUCCEEDED(rv)) {
        prefName.AssignLiteral("font.name.");
        prefName.Append(NS_LossyConvertUTF16toASCII(genericName));
        prefName.AppendLiteral(".");
        prefName.Append(mStyle.langGroup);

        rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(familyName));
        if (NS_SUCCEEDED(rv)) {
            (*fc)(familyName, NS_LossyConvertUTF16toASCII(genericName), closure);
        }
    }
}


gfxFontStyle::gfxFontStyle(PRUint8 aStyle, PRUint8 aVariant,
                           PRUint16 aWeight, PRUint8 aDecoration, gfxFloat aSize,
                           const nsACString& aLangGroup,
                           float aSizeAdjust, PRPackedBool aSystemFont,
                           PRPackedBool aFamilyNameQuirks) :
    style(aStyle), systemFont(aSystemFont), variant(aVariant),
    familyNameQuirks(aFamilyNameQuirks), weight(aWeight),
    decorations(aDecoration), size(PR_MIN(aSize, 5000)), langGroup(aLangGroup), sizeAdjust(aSizeAdjust)
{
    if (langGroup.IsEmpty()) {
        NS_WARNING("empty langgroup");
        langGroup.Assign("x-western");
    }
}

void
gfxFontStyle::ComputeWeightAndOffset(PRInt8 *outBaseWeight, PRInt8 *outOffset) const
{
    PRInt8 baseWeight = (weight + 50) / 100;
    PRInt8 offset = weight - baseWeight * 100;

    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    if (outBaseWeight)
        *outBaseWeight = baseWeight;
    if (outOffset)
        *outOffset = offset;
}
