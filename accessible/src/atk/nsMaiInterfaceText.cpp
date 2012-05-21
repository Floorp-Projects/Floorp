/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "nsHyperTextAccessible.h"
#include "nsMai.h"

#include "nsIPersistentProperties2.h"

using namespace mozilla::a11y;

AtkAttributeSet* ConvertToAtkAttributeSet(nsIPersistentProperties* aAttributes);

static void
ConvertTexttoAsterisks(nsAccessibleWrap* accWrap, nsAString& aString)
{
  // convert each char to "*" when it's "password text" 
  if (accWrap->NativeRole() == roles::PASSWORD_TEXT) {
    for (PRUint32 i = 0; i < aString.Length(); i++)
      aString.Replace(i, 1, NS_LITERAL_STRING("*"));
  }
}

extern "C" {

static gchar*
getTextCB(AtkText *aText, gint aStartOffset, gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

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

static gchar*
getTextAfterOffsetCB(AtkText *aText, gint aOffset,
                     AtkTextBoundary aBoundaryType,
                     gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

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

static gchar*
getTextAtOffsetCB(AtkText *aText, gint aOffset,
                  AtkTextBoundary aBoundaryType,
                  gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

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

static gunichar
getCharacterAtOffsetCB(AtkText* aText, gint aOffset)
{
  nsAccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return 0;

  nsCOMPtr<nsIAccessibleText> accText;
  accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                          getter_AddRefs(accText));
  NS_ENSURE_TRUE(accText, 0);

  // PRUnichar is unsigned short in Mozilla
  // gnuichar is guint32 in glib
  PRUnichar uniChar = 0;
  nsresult rv = accText->GetCharacterAtOffset(aOffset, &uniChar);
  if (NS_FAILED(rv))
    return 0;

  // Convert char to "*" when it's "password text".
  if (accWrap->NativeRole() == roles::PASSWORD_TEXT)
    uniChar = '*';

  return static_cast<gunichar>(uniChar);
}

static gchar*
getTextBeforeOffsetCB(AtkText *aText, gint aOffset,
                      AtkTextBoundary aBoundaryType,
                      gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

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

static gint
getCaretOffsetCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return 0;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, 0);

    PRInt32 offset;
    nsresult rv = accText->GetCaretOffset(&offset);
    return (NS_FAILED(rv)) ? 0 : static_cast<gint>(offset);
}

static AtkAttributeSet*
getRunAttributesCB(AtkText *aText, gint aOffset,
                   gint *aStartOffset,
                   gint *aEndOffset)
{
    *aStartOffset = -1;
    *aEndOffset = -1;

    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsCOMPtr<nsIPersistentProperties> attributes;
    PRInt32 startOffset = 0, endOffset = 0;
    nsresult rv = accText->GetTextAttributes(false, aOffset,
                                             &startOffset, &endOffset,
                                             getter_AddRefs(attributes));
    NS_ENSURE_SUCCESS(rv, nsnull);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    return ConvertToAtkAttributeSet(attributes);
}

static AtkAttributeSet*
getDefaultAttributesCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    nsCOMPtr<nsIPersistentProperties> attributes;
    nsresult rv = accText->GetDefaultTextAttributes(getter_AddRefs(attributes));
    if (NS_FAILED(rv))
        return nsnull;

    return ConvertToAtkAttributeSet(attributes);
}

static void
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

    PRUint32 geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

#ifdef DEBUG
    nsresult rv =
#endif
    accText->GetCharacterExtents(aOffset, &extX, &extY,
                                 &extWidth, &extHeight,
                                 geckoCoordType);
    *aX = extX;
    *aY = extY;
    *aWidth = extWidth;
    *aHeight = extHeight;
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "MaiInterfaceText::GetCharacterExtents, failed\n");
}

static void
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

    PRUint32 geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

#ifdef DEBUG
    nsresult rv =
#endif
    accText->GetRangeExtents(aStartOffset, aEndOffset,
                             &extX, &extY,
                             &extWidth, &extHeight,
                             geckoCoordType);
    aRect->x = extX;
    aRect->y = extY;
    aRect->width = extWidth;
    aRect->height = extHeight;
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "MaiInterfaceText::GetRangeExtents, failed\n");
}

static gint
getCharacterCountCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return 0;

    nsHyperTextAccessible* textAcc = accWrap->AsHyperText();
    return textAcc->IsDefunct() ?
        0 : static_cast<gint>(textAcc->CharacterCount());
}

static gint
getOffsetAtPointCB(AtkText *aText,
                   gint aX, gint aY,
                   AtkCoordType aCoords)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return -1;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, -1);

    PRInt32 offset = 0;
    PRUint32 geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

    accText->GetOffsetAtPoint(aX, aY, geckoCoordType, &offset);
    return static_cast<gint>(offset);
}

static gint
getTextSelectionCountCB(AtkText *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, nsnull);

    PRInt32 selectionCount;
    nsresult rv = accText->GetSelectionCount(&selectionCount);
 
    return NS_FAILED(rv) ? 0 : selectionCount;
}

static gchar*
getTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint *aStartOffset, gint *aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

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
static gboolean
addTextSelectionCB(AtkText *aText,
                   gint aStartOffset,
                   gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->AddSelection(aStartOffset, aEndOffset);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
removeTextSelectionCB(AtkText *aText,
                      gint aSelectionNum)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->RemoveSelection(aSelectionNum);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
setTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint aStartOffset, gint aEndOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->SetSelectionBounds(aSelectionNum,
                                              aStartOffset, aEndOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
setCaretOffsetCB(AtkText *aText, gint aOffset)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return FALSE;

    nsCOMPtr<nsIAccessibleText> accText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText),
                            getter_AddRefs(accText));
    NS_ENSURE_TRUE(accText, FALSE);

    nsresult rv = accText->SetCaretOffset(aOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}
}

void
textInterfaceInitCB(AtkTextIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (NS_UNLIKELY(!aIface))
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
