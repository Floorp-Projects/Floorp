/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { LinkHintsElementSelectedData } from "./linkhints/types.ts";

/** URL schemes considered safe for programmatic navigation from link hints. */
const SAFE_NAVIGATION_SCHEMES = new Set([
  "http",
  "https",
  "ftp",
  "about",
]);

export class NRLinkHintsParent extends JSWindowActorParent {
  receiveMessage(message: { name: string; data?: unknown }): void {
    try {
      switch (message.name) {
        case "LinkHints:ElementSelected": {
          const data = message.data as LinkHintsElementSelectedData | undefined;
          if (!data) return;
          this.handleElementSelected(data);
          break;
        }
        case "LinkHints:Cancelled": {
          // Nothing to do on cancel
          break;
        }
        case "LinkHints:Shown": {
          // Nothing to do, could log for debugging
          break;
        }
      }
    } catch (error) {
      console.error("[LinkHints]", error, { messageName: message.name, data: message.data });
    }
  }

  private handleElementSelected(data: LinkHintsElementSelectedData): void {
    const browser = this.browsingContext?.top?.embedderElement;
    if (!browser) return;

    const win = browser.ownerGlobal;
    if (!win) return;

    switch (data.action) {
      case "openCurrentTab": {
        if (data.href && this.isValidNavigationUrl(data.href)) {
          browser.loadURI(Services.io.newURI(data.href), {
            triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          });
        } else if (data.href) {
          console.warn("[LinkHints] Blocked navigation to unsafe URL:", data.href);
        } else {
          console.debug("[LinkHints] No href for openCurrentTab, element tag:", data.tagName);
        }
        break;
      }
      case "openNewTab": {
        const url = data.href;
        if (!url || !this.isValidNavigationUrl(url)) {
          if (url) console.warn("[LinkHints] Blocked new tab for unsafe URL:", url);
          break;
        }
        win.gBrowser?.addTrustedTab(url);
        break;
      }
      case "openNewBackgroundTab": {
        const url = data.href;
        if (!url || !this.isValidNavigationUrl(url)) {
          if (url) console.warn("[LinkHints] Blocked background tab for unsafe URL:", url);
          break;
        }
        win.gBrowser?.addTrustedTab(url, { inBackground: true });
        break;
      }
      case "copyUrl": {
        if (data.href) {
          navigator.clipboard.writeText(data.href).catch((e: unknown) => {
            console.error("[LinkHints] Failed to copy URL:", e);
          });
        }
        break;
      }
      case "hover": {
        // Hover is handled entirely in the content process by the child actor.
        // This case should not normally be reached.
        break;
      }
    }
  }

  /**
   * Validate that a URL uses a safe scheme for programmatic navigation.
   * Only http, https, ftp, and about schemes are allowed to prevent
   * loading javascript: or chrome: URIs with elevated privileges.
   */
  private isValidNavigationUrl(url: string): boolean {
    try {
      const uri = Services.io.newURI(url);
      return SAFE_NAVIGATION_SCHEMES.has(uri.scheme.toLowerCase());
    } catch {
      return false;
    }
  }
}
