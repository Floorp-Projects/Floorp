/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const NR_WEB_PANEL_CLIPBOARD_READ_MESSAGE = "NRWebPanelClipboard:Read";

/**
 * Shared data key listing the browserIds of web panel content browsers.
 * Written by the panel sidebar chrome code
 * (browser-features/chrome/common/panel-sidebar/utils/web-panel-browser-registry.ts),
 * which keeps its own copy of this string.
 */
export const WEB_PANEL_BROWSER_IDS_SHARED_KEY = "floorp:webPanelBrowserIds";

type NRWebPanelClipboardReadKind = "read" | "readText";

/** Same types and order as Gecko's Clipboard::MandatoryDataTypes(). */
export const WEB_PANEL_CLIPBOARD_FLAVORS: Record<
  NRWebPanelClipboardReadKind,
  readonly string[]
> = {
  read: ["text/html", "text/plain", "image/png"],
  readText: ["text/plain"],
};

export interface NRWebPanelClipboardEntry {
  type: string;
  text?: string;
  bytes?: ArrayBuffer;
}

export type NRWebPanelClipboardReadResult =
  | { status: "ok"; entries: NRWebPanelClipboardEntry[] }
  | { status: "denied" | "no-activation" };

export function isWebPanelClipboardReadKind(
  value: unknown,
): value is NRWebPanelClipboardReadKind {
  return value === "read" || value === "readText";
}

export function isWebPanelBrowserId(
  browserIds: unknown,
  browserId: number | null | undefined,
): boolean {
  return typeof browserId === "number" && browserId > 0 &&
    Array.isArray(browserIds) && browserIds.includes(browserId);
}
