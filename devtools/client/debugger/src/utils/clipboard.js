/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Clipboard function taken from
 * https://dxr.mozilla.org/mozilla-central/source/devtools/shared/platform/content/clipboard.js
 */

// @flow

export function copyToTheClipboard(string: string): void {
  const doCopy = function(e: any) {
    e.clipboardData.setData("text/plain", string);
    e.preventDefault();
  };

  document.addEventListener("copy", doCopy);
  document.execCommand("copy", false, null);
  document.removeEventListener("copy", doCopy);
}
