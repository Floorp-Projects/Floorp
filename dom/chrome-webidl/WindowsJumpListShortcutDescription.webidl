/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A WindowsJumpListShortcutDescription is a structure that describes an entry
 * to be created in the Windows Jump List. Both tasks, as well as custom
 * items can be described using this structure.
 *
 * nsIJumpListBuilder.populateJumpList accepts arrays of these structures.
 */
[GenerateInit, GenerateConversionToJS]
dictionary WindowsJumpListShortcutDescription {
  /**
   * The title of the Jump List item to be displayed to the user.
   */
  required DOMString title;

  /**
   * The path to the executable that Windows should run when the item is
   * selected in the Jump List.
   */
  required DOMString path;

  /**
   * Arguments to be supplied to the executable when the item is selected in
   * the Jump List.
   */
  DOMString arguments;

  /**
   * A description of the item that is displayed as a tooltip.
   */
  required DOMString description;

  /**
   * The path to an icon to assign to the Jump List item. If this is not
   * supplied then the fallbackIconIndex is used instead.
   */
  DOMString iconPath;

  /**
   * The icon index associated with the executable at the path to use in the
   * event that no iconPath is supplied.
   */
  required long fallbackIconIndex;
};
