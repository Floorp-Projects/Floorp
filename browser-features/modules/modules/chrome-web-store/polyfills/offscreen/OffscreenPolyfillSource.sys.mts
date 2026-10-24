/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Kept for callers using the former converter interface. The Runtime supplies
 * chrome.offscreen in every supported extension context; packages need no shim.
 */
export const OFFSCREEN_POLYFILL_SOURCE =
  "/* Offscreen documents are provided by Floorp Runtime. */\n";
