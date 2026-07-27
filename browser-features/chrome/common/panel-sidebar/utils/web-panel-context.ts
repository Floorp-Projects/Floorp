/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const WEB_PANEL_URL_PARAM = "floorpWebPanelId";

/** Whether the given browser chrome URL is an embedded web panel child window. */
export function isWebPanelChildUrl(href: string): boolean {
  try {
    return new URL(href).searchParams.has(WEB_PANEL_URL_PARAM);
  } catch {
    return false;
  }
}

/** Whether the current window is an embedded web panel child window. */
export function isWebPanelChildWindow(): boolean {
  return isWebPanelChildUrl(globalThis.location.href);
}
