/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

namespace mozilla {
namespace a11y {

/**
 * Accessible object types used when creating an accessible based on the frame.
 */
enum AccType {
  eNoType,
  eHTMLBR,
  eHTMLButton,
  eHTMLCanvas,
  eHTMLCaption,
  eHTMLCheckbox,
  eHTMLCombobox,
  eHTMLFileInput,
  eHTMLGroupbox,
  eHTMLHR,
  eHTMLImageMap,
  eHTMLLabel,
  eHTMLLi,
  eHTMLSelectList,
  eHTMLMedia,
  eHTMLRadioButton,
  eHTMLTable,
  eHTMLTableCell,
  eHTMLTableRow,
  eHTMLTextField,
  eHyperText,
  eImage,
  eOuterDoc,
  ePlugin,
  eTextLeaf
};
}
}

