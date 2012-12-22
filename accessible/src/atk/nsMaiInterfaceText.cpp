/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "HyperTextAccessible.h"
#include "nsMai.h"

#include "nsIPersistentProperties2.h"

#include "mozilla/Likely.h"

using namespace mozilla::a11y;

AtkAttributeSet* ConvertToAtkAttributeSet(nsIPersistentProperties* aAttributes);

static void
ConvertTexttoAsterisks(AccessibleWrap* accWrap, nsAString& aString)
{
  // convert each char to "*" when it's "password text" 
  if (accWrap->NativeRole() == roles::PASSWORD_TEXT) {
    for (uint32_t i = 0; i < aString.Length(); i++)
      aString.Replace(i, 1, NS_LITERAL_STRING("*"));
  }
}

extern "C" {

static gchar*
getTextCB(AtkText *aText, gint aStartOffset, gint aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    nsAutoString autoStr;
    nsresult rv = text->GetText(aStartOffset, aEndOffset, autoStr);
    NS_ENSURE_SUCCESS(rv, nullptr);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);

    //copy and return, libspi will free it.
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nullptr;
}

static gchar*
getTextAfterOffsetCB(AtkText *aText, gint aOffset,
                     AtkTextBoundary aBoundaryType,
                     gint *aStartOffset, gint *aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

  nsAutoString autoStr;
  int32_t startOffset = 0, endOffset = 0;
  nsresult rv =
    text->GetTextAfterOffset(aOffset, aBoundaryType,
                             &startOffset, &endOffset, autoStr);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  NS_ENSURE_SUCCESS(rv, nullptr);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nullptr;
}

static gchar*
getTextAtOffsetCB(AtkText *aText, gint aOffset,
                  AtkTextBoundary aBoundaryType,
                  gint *aStartOffset, gint *aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    nsAutoString autoStr;
    int32_t startOffset = 0, endOffset = 0;
    nsresult rv =
        text->GetTextAtOffset(aOffset, aBoundaryType,
                              &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nullptr);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nullptr;
}

static gunichar
getCharacterAtOffsetCB(AtkText* aText, gint aOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return 0;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return 0;

  // PRUnichar is unsigned short in Mozilla
  // gnuichar is guint32 in glib
  PRUnichar uniChar = 0;
  nsresult rv = text->GetCharacterAtOffset(aOffset, &uniChar);
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
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    nsAutoString autoStr;
    int32_t startOffset = 0, endOffset = 0;
    nsresult rv =
        text->GetTextBeforeOffset(aOffset, aBoundaryType,
                                  &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nullptr);

    ConvertTexttoAsterisks(accWrap, autoStr);
    NS_ConvertUTF16toUTF8 cautoStr(autoStr);
    return (cautoStr.get()) ? g_strdup(cautoStr.get()) : nullptr;
}

static gint
getCaretOffsetCB(AtkText *aText)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return 0;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return 0;

    int32_t offset;
    nsresult rv = text->GetCaretOffset(&offset);
    return (NS_FAILED(rv)) ? 0 : static_cast<gint>(offset);
}

static AtkAttributeSet*
getRunAttributesCB(AtkText *aText, gint aOffset,
                   gint *aStartOffset,
                   gint *aEndOffset)
{
  *aStartOffset = -1;
  *aEndOffset = -1;

  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    nsCOMPtr<nsIPersistentProperties> attributes;
    int32_t startOffset = 0, endOffset = 0;
    nsresult rv = text->GetTextAttributes(false, aOffset,
                                          &startOffset, &endOffset,
                                          getter_AddRefs(attributes));
    NS_ENSURE_SUCCESS(rv, nullptr);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    return ConvertToAtkAttributeSet(attributes);
}

static AtkAttributeSet*
getDefaultAttributesCB(AtkText *aText)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    nsCOMPtr<nsIPersistentProperties> attributes;
    nsresult rv = text->GetDefaultTextAttributes(getter_AddRefs(attributes));
    if (NS_FAILED(rv))
        return nullptr;

    return ConvertToAtkAttributeSet(attributes);
}

static void
getCharacterExtentsCB(AtkText *aText, gint aOffset,
                      gint *aX, gint *aY,
                      gint *aWidth, gint *aHeight,
                      AtkCoordType aCoords)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if(!accWrap || !aX || !aY || !aWidth || !aHeight)
    return;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return;

    int32_t extY = 0, extX = 0;
    int32_t extWidth = 0, extHeight = 0;

    uint32_t geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

#ifdef DEBUG
    nsresult rv =
#endif
    text->GetCharacterExtents(aOffset, &extX, &extY,
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
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if(!accWrap || !aRect)
    return;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return;

    int32_t extY = 0, extX = 0;
    int32_t extWidth = 0, extHeight = 0;

    uint32_t geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

#ifdef DEBUG
    nsresult rv =
#endif
    text->GetRangeExtents(aStartOffset, aEndOffset,
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
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return 0;

  HyperTextAccessible* textAcc = accWrap->AsHyperText();
  return textAcc->IsDefunct() ?
    0 : static_cast<gint>(textAcc->CharacterCount());
}

static gint
getOffsetAtPointCB(AtkText *aText,
                   gint aX, gint aY,
                   AtkCoordType aCoords)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return -1;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return -1;

    int32_t offset = 0;
    uint32_t geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

    text->GetOffsetAtPoint(aX, aY, geckoCoordType, &offset);
    return static_cast<gint>(offset);
}

static gint
getTextSelectionCountCB(AtkText *aText)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return 0;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return 0;

    int32_t selectionCount;
    nsresult rv = text->GetSelectionCount(&selectionCount);
 
    return NS_FAILED(rv) ? 0 : selectionCount;
}

static gchar*
getTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint *aStartOffset, gint *aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return nullptr;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return nullptr;

    int32_t startOffset = 0, endOffset = 0;
    nsresult rv = text->GetSelectionBounds(aSelectionNum,
                                           &startOffset, &endOffset);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

    NS_ENSURE_SUCCESS(rv, nullptr);

    return getTextCB(aText, *aStartOffset, *aEndOffset);
}

// set methods
static gboolean
addTextSelectionCB(AtkText *aText,
                   gint aStartOffset,
                   gint aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return false;

    nsresult rv = text->AddSelection(aStartOffset, aEndOffset);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
removeTextSelectionCB(AtkText *aText,
                      gint aSelectionNum)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return false;

    nsresult rv = text->RemoveSelection(aSelectionNum);

    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
setTextSelectionCB(AtkText *aText, gint aSelectionNum,
                   gint aStartOffset, gint aEndOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return false;

    nsresult rv = text->SetSelectionBounds(aSelectionNum,
                                           aStartOffset, aEndOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}

static gboolean
setCaretOffsetCB(AtkText *aText, gint aOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole())
    return false;

    nsresult rv = text->SetCaretOffset(aOffset);
    return NS_SUCCEEDED(rv) ? TRUE : FALSE;
}
}

void
textInterfaceInitCB(AtkTextIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid aIface");
  if (MOZ_UNLIKELY(!aIface))
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
