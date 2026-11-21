/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Publishes the browserIds of web panel content browsers to all processes.
 *
 * Content-side actors cannot see that a page is embedded in a web panel's
 * nested browser.xhtml, but they can read shared data synchronously before
 * page scripts run. NRWebPanelClipboard uses this list to enable its
 * clipboard read fallback only in web panels.
 */

import type { WritableSharedDataLike } from "./web-panel-types.ts";

/** Keep in sync with WEB_PANEL_BROWSER_IDS_SHARED_KEY in
 * browser-features/modules/common/NRWebPanelClipboardTypes.ts. */
export const WEB_PANEL_BROWSER_IDS_SHARED_KEY = "floorp:webPanelBrowserIds";

function defaultSharedData(): WritableSharedDataLike {
  return Services.ppmm.sharedData as unknown as WritableSharedDataLike;
}

function readBrowserIds(sharedData: WritableSharedDataLike): number[] {
  const value = sharedData.get(WEB_PANEL_BROWSER_IDS_SHARED_KEY);
  return Array.isArray(value)
    ? value.filter((id): id is number => typeof id === "number")
    : [];
}

function writeBrowserIds(
  sharedData: WritableSharedDataLike,
  browserIds: number[],
): void {
  if (browserIds.length) {
    sharedData.set(WEB_PANEL_BROWSER_IDS_SHARED_KEY, browserIds);
  } else {
    sharedData.delete(WEB_PANEL_BROWSER_IDS_SHARED_KEY);
  }
}

/** A missing id (0) must never match an uninitialized browser. */
function isValidBrowserId(browserId: number): boolean {
  return Number.isSafeInteger(browserId) && browserId > 0;
}

export function registerWebPanelBrowserId(
  browserId: number,
  sharedData: WritableSharedDataLike = defaultSharedData(),
): void {
  if (!isValidBrowserId(browserId)) {
    return;
  }
  const browserIds = readBrowserIds(sharedData);
  if (browserIds.includes(browserId)) {
    return;
  }
  writeBrowserIds(sharedData, [...browserIds, browserId]);
  // Flush now so content processes see the id before the panel page loads.
  sharedData.flush();
}

export function unregisterWebPanelBrowserId(
  browserId: number,
  sharedData: WritableSharedDataLike = defaultSharedData(),
): void {
  const browserIds = readBrowserIds(sharedData);
  if (!browserIds.includes(browserId)) {
    return;
  }
  // No flush: a stale id is harmless (the parent re-checks every request),
  // and the next idle flush publishes the removal.
  writeBrowserIds(sharedData, browserIds.filter((id) => id !== browserId));
}
