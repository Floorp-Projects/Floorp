/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccTypes_h
#define mozilla_a11y_AccTypes_h

namespace mozilla {
namespace a11y {

/**
 * Accessible object types. Each accessible class can have own type.
 */
enum AccType {
  /**
   * This set of types is used for accessible creation, keep them together in
   * alphabetical order since they are used in switch statement.
   */
  eNoType,
  eHTMLBRType,
  eHTMLButtonType,
  eHTMLCanvasType,
  eHTMLCaptionType,
  eHTMLCheckboxType,
  eHTMLComboboxType,
  eHTMLFileInputType,
  eHTMLGroupboxType,
  eHTMLHRType,
  eHTMLImageMapType,
  eHTMLLiType,
  eHTMLSelectListType,
  eHTMLMediaType,
  eHTMLRadioButtonType,
  eHTMLRangeType,
  eHTMLSpinnerType,
  eHTMLTableType,
  eHTMLTableCellType,
  eHTMLTableRowType,
  eHTMLTextFieldType,
  eHyperTextType,
  eImageType,
  eOuterDocType,
  ePluginType,
  eTextLeafType,

  /**
   * Other accessible types.
   */
  eApplicationType,
  eHTMLOptGroupType,
  eImageMapType,
  eMenuPopupType,
  eProgressType,
  eRootType,
  eXULLabelType,
  eXULListItemType,
  eXULTabpanelsType,
  eXULTreeType,

  eLastAccType = eXULTreeType
};

/**
 * Generic accessible type, different accessible classes can share the same
 * type, the same accessible class can have several types.
 */
enum AccGenericType {
  eAutoComplete = 1 << 0,
  eAutoCompletePopup = 1 << 1,
  eButton = 1 << 2,
  eCombobox = 1 << 3,
  eDocument = 1 << 4,
  eHyperText = 1 << 5,
  eList = 1 << 6,
  eListControl = 1 << 7,
  eMenuButton = 1 << 8,
  eSelect = 1 << 9,
  eTable = 1 << 10,
  eTableCell = 1 << 11,
  eTableRow = 1 << 12,

  eLastAccGenericType = eTableRow
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_AccTypes_h
