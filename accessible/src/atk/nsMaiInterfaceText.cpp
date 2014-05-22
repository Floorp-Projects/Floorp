/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "nsMai.h"

#include "nsIAccessibleTypes.h"
#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/Likely.h"

using namespace mozilla;
using namespace mozilla::a11y;

static const char* sAtkTextAttrNames[ATK_TEXT_ATTR_LAST_DEFINED];

static AtkAttributeSet*
ConvertToAtkTextAttributeSet(nsIPersistentProperties* aAttributes)
{
  if (!aAttributes)
    return nullptr;

  AtkAttributeSet* objAttributeSet = nullptr;
  nsCOMPtr<nsISimpleEnumerator> propEnum;
  nsresult rv = aAttributes->Enumerate(getter_AddRefs(propEnum));
  NS_ENSURE_SUCCESS(rv, nullptr);

  bool hasMore = false;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> sup;
    rv = propEnum->GetNext(getter_AddRefs(sup));
    NS_ENSURE_SUCCESS(rv, objAttributeSet);

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(sup));
    NS_ENSURE_TRUE(propElem, objAttributeSet);

    nsAutoCString name;
    rv = propElem->GetKey(name);
    NS_ENSURE_SUCCESS(rv, objAttributeSet);

    nsAutoString value;
    rv = propElem->GetValue(value);
    NS_ENSURE_SUCCESS(rv, objAttributeSet);

    AtkAttribute* objAttr = (AtkAttribute*)g_malloc(sizeof(AtkAttribute));
    objAttr->name = g_strdup(name.get());
    objAttr->value = g_strdup(NS_ConvertUTF16toUTF8(value).get());
    objAttributeSet = g_slist_prepend(objAttributeSet, objAttr);

    // Handle attributes where atk has its own name.
    const char* atkName = nullptr;
    nsAutoString atkValue;
    if (name.EqualsLiteral("color")) {
      // The format of the atk attribute is r,g,b and the gecko one is
      // rgb(r,g,b).
      atkValue = Substring(value, 5, value.Length() - 1);
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_FG_COLOR];
    } else if (name.EqualsLiteral("background-color")) {
      // The format of the atk attribute is r,g,b and the gecko one is
      // rgb(r,g,b).
      atkValue = Substring(value, 5, value.Length() - 1);
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_BG_COLOR];
    } else if (name.EqualsLiteral("font-family")) {
      atkValue = value;
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_FAMILY_NAME];
    } else if (name.EqualsLiteral("font-size")) {
      // ATK wants the number of pixels without px at the end.
      atkValue = StringHead(value, value.Length() - 2);
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_SIZE];
    } else if (name.EqualsLiteral("font-weight")) {
      atkValue = value;
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_WEIGHT];
    } else if (name.EqualsLiteral("invalid")) {
      atkValue = value;
      atkName = sAtkTextAttrNames[ATK_TEXT_ATTR_INVALID];
    }

    if (atkName) {
      objAttr = static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
      objAttr->name = g_strdup(atkName);
      objAttr->value = g_strdup(NS_ConvertUTF16toUTF8(atkValue).get());
      objAttributeSet = g_slist_prepend(objAttributeSet, objAttr);
    }
  }

  // libatk-adaptor will free it
  return objAttributeSet;
}

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
    text->TextSubstring(aStartOffset, aEndOffset, autoStr);

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
  text->TextAfterOffset(aOffset, aBoundaryType, &startOffset, &endOffset, autoStr);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

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
    text->TextAtOffset(aOffset, aBoundaryType, &startOffset, &endOffset, autoStr);
    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

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

  // char16_t is unsigned short in Mozilla, gnuichar is guint32 in glib.
  return static_cast<gunichar>(text->CharAt(aOffset));
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
  text->TextBeforeOffset(aOffset, aBoundaryType,
                         &startOffset, &endOffset, autoStr);
  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

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

  return static_cast<gint>(text->CaretOffset());
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

  int32_t startOffset = 0, endOffset = 0;
  nsCOMPtr<nsIPersistentProperties> attributes =
    text->TextAttributes(false, aOffset, &startOffset, &endOffset);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return ConvertToAtkTextAttributeSet(attributes);
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

  nsCOMPtr<nsIPersistentProperties> attributes = text->DefaultTextAttributes();
  return ConvertToAtkTextAttributeSet(attributes);
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

    uint32_t geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

  nsIntRect rect = text->CharBounds(aOffset, geckoCoordType);
  *aX = rect.x;
  *aY = rect.y;
  *aWidth = rect.width;
  *aHeight = rect.height;
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

    uint32_t geckoCoordType;
    if (aCoords == ATK_XY_SCREEN)
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE;
    else
        geckoCoordType = nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE;

  nsIntRect rect = text->TextBounds(aStartOffset, aEndOffset, geckoCoordType);
  aRect->x = rect.x;
  aRect->y = rect.y;
  aRect->width = rect.width;
  aRect->height = rect.height;
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

  return static_cast<gint>(
    text->OffsetAtPoint(aX, aY,
                        (aCoords == ATK_XY_SCREEN ?
                         nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
                         nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE)));
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

  return text->SelectionCount();
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
  text->SelectionBoundsAt(aSelectionNum, &startOffset, &endOffset);

    *aStartOffset = startOffset;
    *aEndOffset = endOffset;

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
    return FALSE;

  return text->AddToSelection(aStartOffset, aEndOffset);
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
    return FALSE;

  return text->RemoveFromSelection(aSelectionNum);
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
    return FALSE;

  return text->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
}

static gboolean
setCaretOffsetCB(AtkText *aText, gint aOffset)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
  if (!accWrap)
    return FALSE;

  HyperTextAccessible* text = accWrap->AsHyperText();
  if (!text || !text->IsTextRole() || !text->IsValidOffset(aOffset))
    return FALSE;

  text->SetCaretOffset(aOffset);
  return TRUE;
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

  // Cache the string values of the atk text attribute names.
  for (uint32_t i = 0; i < ArrayLength(sAtkTextAttrNames); i++)
    sAtkTextAttrNames[i] =
      atk_text_attribute_get_name(static_cast<AtkTextAttribute>(i));
}
