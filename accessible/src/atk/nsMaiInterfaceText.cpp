/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *   Ginn Chen (ginn.chen@sun.com)
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

#include "nsMaiInterfaceText.h"
#include "nsString.h"

AtkAttributeSet * GetAttributeSet(nsIAccessible* aAccessible);

void
textInterfaceInitCB(AtkTextIface *aIface)
{
    NS_ASSERTION(aIface, "Invalid aIface");
    if (!aIface)
        return;

    aIface->get_text = getTextCB;
    aIface->get_text_after_offset = getTextAfterOffsetCB;
    aIface->get_text_at_offset = getTextAtOffsetCB;
    aIface->get_character_at_offset = getCharacterAtOffsetCB;
    aIface->get_text_before_offset = getTextBeforeOffsetCB;
    aIface->get_caret_offset = getCaretOffsetCB;
    aIface->get_run_attributes = getRunAttributesCB;
    aIface->get_default_attributes = getDefaultAttributesCB;
    aIface->get_character_extents = getCharacterExtentsCB;
    aIface->get_range_extents = getRangeExtentsCB;
    aIface->get_character_count = getCharacterCountCB;
    aIface->get_offset_at_point = getOffsetAtPointCB;
    aIface->get_n_selections = getTextSelectionCountCB;
    aIface->get_selection = getTextSelectionCB;

    // set methods
    aIface->add_selection = addTextSelectionCB;
    aIface->remove_selection = removeTextSelectionCB;
    aIface->set_selection = setTextSelectionCB;
    aIface->set_caret_offset = setCaretOffsetCB;
}


void ConvertTexttoAsterisks(nsAccessibleWrap* accWrap, nsAString& aString)
{
    // convert each char to "*" when it's "password text" 
    PRUint32 accRole;
    accWrap->GetRole(&accRole);
    if (NS_STATIC_CAST(AtkRole, accRole) == ATK_ROLE_PASSWORD_TEXT) {
        for (PRUint32 i = 0; i < aString.Length(); i++)
            aString.Replace(i, 1, NS_LITERAL_STRING("*"));
    }
}

gchar *
getTextCB(AtkText *aText, gint aStartOffset, gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsAutoString autoStr;
    nsresult rv = accText->GetText(aStartOffset, aEndOffset, autoStr);
    NS_ENSURE_SUCCESS(rv, nsnull);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);

    //copy and return, libspi will free it.
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nsnull;
}

gchar *
getTextAfterOffsetCB(AtkText *aText, gint aOffset,
                     AtkTextBoundary aBoundaryType,
                     gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accText->GetTextAfterOffset(aOffset, aBoundaryType,
                                    &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nsnull);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nsnull;
}

gchar *
getTextAtOffsetCB(AtkText *aText, gint aOffset,
                  AtkTextBoundary aBoundaryType,
                  gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accText->GetTextAtOffset(aOffset, aBoundaryType,
                                 &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nsnull);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nsnull;
}

gunichar
getCharacterAtOffsetCB(AtkText *aText, gint aOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, 0);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, 0);

    /* PRUnichar is unsigned short in Mozilla */
    /* gnuichar is guint32 in glib */
    PRUnichar uniChar;
    nsresult rv =
        accText->GetCharacterAtOffset(aOffset, &uniChar);

    // convert char to "*" when it's "password text" 
    PRUint32 accRole;
    accWrap->GetRole(&accRole);
    if (NS_STATIC_CAST(AtkRole, accRole) == ATK_ROLE_PASSWORD_TEXT) {
        uniChar = '*';
    }

    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gunichar, uniChar);
}

gchar *
getTextBeforeOffsetCB(AtkText *aText, gint aOffset,
                      AtkTextBoundary aBoundaryType,
                      gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsAutoString autoStr;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv =
        accText->GetTextBeforeOffset(aOffset, aBoundaryType,
                                     &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nsnull);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nsnull;
}

gint
getCaretOffsetCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, 0);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, 0);

    PRInt32 offset;
    nsresult rv = accText->GetCaretOffset(&offset);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, offset);
}

AtkAttributeSet *
getRunAttributesCB(AtkText *aText, gint aOffset,
                   gint *aStartOffset,
                   gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsCOMPtr<nsIAccessible> accessibleWithAttrs;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv = accText->GetAttributeRange(aOffset,
                                             &startOffset, &endOffset,
                                             getter_AddRefs(accessibleWithAttrs));
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;
    NS_ENSURE_SUCCESS(rv, nsnull);

    return GetAttributeSet(accessibleWithAttrs);
}

AtkAttributeSet *
getDefaultAttributesCB(AtkText *aText)
{
    /* not supported ??? */
    return nsnull;
}

void
getCharacterExtentsCB(AtkText *aText, gint aOffset,
                      gint *aX, gint *aY,
                      gint *aWidth, gint *aHeight,
                      AtkCoordType aCoords)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if(!accWrap || !aX || !aY || !aWidth || !aHeight)
        return;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    PRInt32 extY = 0, extX = 0;
    PRInt32 extWidth = 0, extHeight = 0;
    nsresult rv = accText->GetCharacterExtents(aOffset, &extX, &extY,
                                               &extWidth, &extHeight,
                                               aCoords);
    *aX = extX;
    *aY = extY;
    *aWidth = extWidth;
    *aHeight = extHeight;
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "MaiInterfaceText::GetCharacterExtents, failed\n");
}

void
getRangeExtentsCB(AtkText *aText, gint aStartOffset, gint aEndOffset,
                  AtkCoordType aCoords, AtkTextRectangle *aRect)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if(!accWrap || !aRect)
        return;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    if (!accText)
        return;

    PRInt32 extY = 0, extX = 0;
    PRInt32 extWidth = 0, extHeight = 0;
    nsresult rv = accText->GetRangeExtents(aStartOffset, aEndOffset,
                                           &extX, &extY,
                                           &extWidth, &extHeight,
                                           aCoords);
    aRect->x = extX;
    aRect->y = extY;
    aRect->width = extWidth;
    aRect->height = extHeight;
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "MaiInterfaceText::GetRangeExtents, failed\n");
}

gint
getCharacterCountCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, 0);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, 0);

    PRInt32 count = 0;
    nsresult rv = accText->GetCharacterCount(&count);
    return (NS_FAILED(rv)) ? 0 : NS_STATIC_CAST(gint, count);
}

gint
getOffsetAtPointCB(AtkText *aText,
                   gint aX, gint aY,
                   AtkCoordType aCoords)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, -1);

    PRInt32 offset = 0;
    accText->GetOffsetAtPoint(aX, aY, aCoords, &offset);
    return NS_STATIC_CAST(gint, offset);
}

gint
getTextSelectionCountCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));

    PRInt32 selectionCount;
    nsresult rv = accText->GetSelectionCount(&selectionCount);
 
    return NS_FAILED(rv) ? 0 : selectionCount;
}

gchar *
getTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv = accText->GetSelectionBounds(aSelectionNum,
                                              &startOffset, &endOffset);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nsnull);

    return getTextCB(aText, *aStartOffset, *aEndOffset);
}

// set methods
gboolean
addTextSelectionCB(AtkText *aText,
                   gint aStartOffset,
                   gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->AddSelection(aStartOffset, aEndOffset);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

gboolean
removeTextSelectionCB(AtkText *aText,
                      gint aSelectionNum)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->RemoveSelection(aSelectionNum);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

gboolean
setTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint aStartOffset, gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->SetSelectionBounds(aSelectionNum,
                                              aStartOffset, aEndOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

gboolean
setCaretOffsetCB(AtkText *aText, gint aOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, FALSE);

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->SetCaretOffset(aOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}
