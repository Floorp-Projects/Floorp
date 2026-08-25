/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const FLOORP_ADD_EXTENSION_BUTTON_ID = "floorp-add-extension-btn";

/**
 * Find the primary visible action in a Chrome Web Store extension header.
 *
 * Hidden utility controls and the Share button precede the install action in
 * the current store DOM. Scanning in reverse selects the install action without
 * relying on localized text or generated class names.
 */
export function findPrimaryVisibleActionButton(
  header: Element,
  isVisible: (button: HTMLButtonElement) => boolean,
): HTMLButtonElement | null {
  const buttons = header.querySelectorAll<HTMLButtonElement>("button");
  for (let index = buttons.length - 1; index >= 0; index--) {
    const button = buttons.item(index);
    if (
      button.id !== FLOORP_ADD_EXTENSION_BUTTON_ID &&
      isVisible(button)
    ) {
      return button;
    }
  }

  return null;
}
