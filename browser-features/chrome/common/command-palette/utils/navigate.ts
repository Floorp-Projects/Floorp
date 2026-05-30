// SPDX-License-Identifier: MPL-2.0

import type { ChromeWindow } from "../types.ts";

/**
 * Navigate the browser to a URL using gBrowser.loadURI with the current page's
 * triggering principal. Shared across command palette providers and switchers.
 */
export function navigateToUrl(
  win: Window,
  url: string,
  logPrefix = "[command-palette]",
): void {
  try {
    const { gBrowser } = win as unknown as ChromeWindow;
    const principal = gBrowser?.selectedBrowser?.contentPrincipal;
    gBrowser?.loadURI?.(Services.io.newURI(url), {
      triggeringPrincipal: principal,
    });
  } catch (e) {
    console.error(`${logPrefix} Navigation failed for ${url}`, e);
  }
}
