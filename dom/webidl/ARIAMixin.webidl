/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/aria/#ARIAMixin
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface mixin ARIAMixin {
  [Pref="accessibility.ARIAElementReflection.enabled", CEReactions]
  attribute Element? ariaActiveDescendantElement;

  [CEReactions, SetterThrows]
  attribute DOMString? role;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaAtomic;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaAutoComplete;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaBrailleLabel;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaBrailleRoleDescription;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaBusy;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaChecked;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaColCount;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaColIndex;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaColIndexText;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaColSpan;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaCurrent;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaDescription;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaDisabled;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaExpanded;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaHasPopup;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaHidden;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaInvalid;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaKeyShortcuts;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaLabel;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaLevel;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaLive;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaModal;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaMultiLine;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaMultiSelectable;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaOrientation;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaPlaceholder;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaPosInSet;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaPressed;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaReadOnly;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRelevant;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRequired;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRoleDescription;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRowCount;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRowIndex;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRowIndexText;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaRowSpan;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaSelected;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaSetSize;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaSort;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaValueMax;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaValueMin;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaValueNow;

  [CEReactions, SetterThrows]
  attribute DOMString? ariaValueText;
};
