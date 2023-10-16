/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://rawgit.com/w3c/aria/master/#AriaAttributes
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface mixin AriaAttributes {
  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaAtomic;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaAutoComplete;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaBusy;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaChecked;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaColCount;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaColIndex;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaColIndexText;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaColSpan;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaCurrent;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaDescription;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaDisabled;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaExpanded;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaHasPopup;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaHidden;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaInvalid;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaKeyShortcuts;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaLabel;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaLevel;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaLive;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaModal;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaMultiLine;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaMultiSelectable;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaOrientation;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaPlaceholder;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaPosInSet;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaPressed;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaReadOnly;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRelevant;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRequired;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRoleDescription;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRowCount;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRowIndex;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRowIndexText;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaRowSpan;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaSelected;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaSetSize;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaSort;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaValueMax;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaValueMin;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaValueNow;

  [Pref="accessibility.ARIAReflection.enabled", CEReactions, SetterThrows]
  attribute DOMString? ariaValueText;
};
