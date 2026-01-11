/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebScraperParent extends JSWindowActorParent {
  private contextMenuBlocked = false;
  private contextMenuHandler: ((e: Event) => void) | null = null;

  receiveMessage(message: { name: string; data?: Record<string, unknown> }) {
    // Handle translation requests in parent process (async)
    if (message.name === "WebScraper:Translate") {
      return this.handleTranslate(message.data);
    }

    // Handle control overlay context menu blocking
    if (message.name === "WebScraper:BlockContextMenu") {
      this.blockContextMenu();
      return true;
    }

    if (message.name === "WebScraper:UnblockContextMenu") {
      this.unblockContextMenu();
      return true;
    }

    // Forward all other messages to the child and return the result.
    return this.sendQuery(message.name, message.data);
  }

  private blockContextMenu(): void {
    if (this.contextMenuBlocked) return;

    const browser = this.browsingContext?.top?.embedderElement;
    if (!browser) return;

    const win = browser.ownerGlobal;
    if (!win?.document) return;

    const contextMenu = win.document.getElementById("contentAreaContextMenu");
    if (!contextMenu) return;

    this.contextMenuHandler = (e: Event) => {
      e.preventDefault();
      e.stopPropagation();
      const target = e.target as Element & { hidePopup?: () => void };
      target?.hidePopup?.();
    };

    contextMenu.addEventListener("popupshowing", this.contextMenuHandler, true);
    this.contextMenuBlocked = true;
  }

  private unblockContextMenu(): void {
    if (!this.contextMenuBlocked || !this.contextMenuHandler) return;

    const browser = this.browsingContext?.top?.embedderElement;
    if (!browser) return;

    const win = browser.ownerGlobal;
    if (!win?.document) return;

    const contextMenu = win.document.getElementById("contentAreaContextMenu");
    if (contextMenu) {
      contextMenu.removeEventListener(
        "popupshowing",
        this.contextMenuHandler,
        true,
      );
    }

    this.contextMenuHandler = null;
    this.contextMenuBlocked = false;
  }

  didDestroy(): void {
    this.unblockContextMenu();
  }

  /**
   * Handle translation requests asynchronously
   * Waits for translation provider to be initialized before translating
   */
  private async handleTranslate(
    data: Record<string, unknown> | undefined,
  ): Promise<string | null> {
    try {
      const { I18nUtils } = ChromeUtils.importESModule(
        "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
      );

      // Wait for provider to be initialized (max 2 seconds)
      await I18nUtils.waitForProviderInit(2000);

      const provider = I18nUtils.getTranslationProvider();
      if (provider && typeof provider.t === "function") {
        const key = data?.key as string;
        const vars = (data?.vars as Record<string, string | number>) || {};
        const result = provider.t(key, vars);
        if (Array.isArray(result)) {
          return result
            .map((entry) => (typeof entry === "string" ? entry : String(entry)))
            .join(" ");
        }
        if (typeof result === "string" && result.trim().length > 0) {
          return result;
        }
      }
    } catch {
      // ignore translation errors
    }
    return null;
  }
}
