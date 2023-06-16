/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get the chrome (ie, browser) window hosting this content.
export function getChromeWindow(window) {
  return window.browsingContext.topChromeWindow;
}
