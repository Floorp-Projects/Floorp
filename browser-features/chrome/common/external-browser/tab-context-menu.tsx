/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { externalBrowserService } from "./external-browser-service.ts";
import type { ExternalBrowser } from "./types.ts";

const MENU_ID = "context_openInExternalBrowser";
const MENU_POPUP_ID = "context_openInExternalBrowser_popup";
const MARKER_ID = "context_moveTabOptions";

// Helper function for i18n translation
const t = (key: string): string => (i18next.t as (k: string) => string)(key);

/**
 * Tab context menu for opening current tab in external browser
 */
export class ExternalBrowserTabContextMenu {
  private cachedBrowsers: ExternalBrowser[] = [];

  constructor() {
    const parentElem = document?.getElementById("tabContextMenu");
    if (!parentElem) {
      console.error(
        "[ExternalBrowserTabContextMenu] #tabContextMenu not found; skip menu injection.",
      );
      return;
    }

    const marker = document?.getElementById(MARKER_ID);

    try {
      render(() => this.menu(), parentElem, {
        marker: marker?.parentElement === parentElem ? marker : undefined,
      });
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[ExternalBrowserTabContextMenu] Failed to render context menu.",
        reason,
      );
      return;
    }

    // Load browsers on init
    this.loadBrowsers();

    addI18nObserver(() => {
      this.updateLabels();
    });
  }

  private async loadBrowsers(): Promise<void> {
    try {
      this.cachedBrowsers = await externalBrowserService.getInstalledBrowsers();
    } catch (error) {
      console.error(
        "[ExternalBrowserTabContextMenu] Failed to load browsers:",
        error,
      );
      this.cachedBrowsers = [];
    }
  }

  private updateLabels(): void {
    const menuElem = document?.getElementById(MENU_ID);
    if (menuElem) {
      menuElem.setAttribute("label", t("externalBrowser.menu.openTabIn"));
    }
  }

  private menu() {
    return (
      <xul:menu id={MENU_ID} label={t("externalBrowser.menu.openTabIn")}>
        <xul:menupopup
          id={MENU_POPUP_ID}
          onPopupShowing={() => this.populateMenu()}
        />
      </xul:menu>
    );
  }

  private async populateMenu(): Promise<void> {
    const popup = document?.getElementById(MENU_POPUP_ID) as XULElement | null;
    if (!popup) {
      return;
    }

    // Clear existing items
    while (popup.firstChild) {
      popup.removeChild(popup.firstChild);
    }

    // Refresh browsers list
    await this.loadBrowsers();

    // Get the URL of the context tab
    const contextTab = window.TabContextMenu.contextTab;
    const browser = window.gBrowser.getBrowserForTab(contextTab);
    const url = browser?.currentURI?.spec || "";

    if (!url || url === "about:blank" || url === "about:newtab") {
      const emptyItem = this.createMenuItem(
        t("externalBrowser.menu.noValidUrl"),
        true,
      );
      if (emptyItem) {
        popup.appendChild(emptyItem);
      }
      return;
    }

    if (this.cachedBrowsers.length === 0) {
      const emptyItem = this.createMenuItem(
        t("externalBrowser.menu.noBrowsers"),
        true,
      );
      if (emptyItem) {
        popup.appendChild(emptyItem);
      }
      return;
    }

    // Add "Open in default browser" option
    const defaultItem = this.createMenuItem(
      t("externalBrowser.menu.defaultBrowser"),
      false,
      () => this.openInBrowser(url),
    );
    if (defaultItem) {
      popup.appendChild(defaultItem);
    }

    // Add separator
    const separator = document?.createXULElement?.("menuseparator");
    if (separator) {
      popup.appendChild(separator);
    }

    // Add each detected browser
    for (const browserInfo of this.cachedBrowsers) {
      const menuItem = this.createMenuItem(
        browserInfo.name,
        false,
        () => this.openInBrowser(url, browserInfo.id),
      );
      if (menuItem) {
        popup.appendChild(menuItem);
      }
    }
  }

  private createMenuItem(
    label: string,
    disabled: boolean,
    onClick?: () => void,
  ): XULElement | null {
    const menuitem = document?.createXULElement?.("menuitem") as
      | XULElement
      | undefined;
    if (!menuitem) {
      return null;
    }
    menuitem.setAttribute("label", label);
    if (disabled) {
      menuitem.setAttribute("disabled", "true");
    }
    if (onClick) {
      menuitem.addEventListener("command", onClick);
    }
    return menuitem;
  }

  private async openInBrowser(url: string, browserId?: string): Promise<void> {
    try {
      const result = browserId
        ? await externalBrowserService.openUrl(url, browserId)
        : await externalBrowserService.openInDefaultBrowser(url);

      if (!result.success) {
        console.error(
          "[ExternalBrowserTabContextMenu] Failed to open URL:",
          result.error,
        );
      }
    } catch (error) {
      console.error(
        "[ExternalBrowserTabContextMenu] Error opening URL:",
        error,
      );
    }
  }
}
