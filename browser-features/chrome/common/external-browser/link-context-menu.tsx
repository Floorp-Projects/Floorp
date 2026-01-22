/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { externalBrowserService } from "./external-browser-service.ts";
import type { ExternalBrowser } from "./types.ts";

const MENU_ID = "context_openLinkInExternalBrowser";
const MENU_POPUP_ID = "context_openLinkInExternalBrowser_popup";
const LINK_OPEN_MENU_ID = "context-openlink";

// Helper function for i18n translation
const t = (key: string): string => (i18next.t as (k: string) => string)(key);

/**
 * Link context menu for opening links in external browser
 */
export class ExternalBrowserLinkContextMenu {
  private cachedBrowsers: ExternalBrowser[] = [];
  private observer: MutationObserver | null = null;
  private readonly contentContextMenu: XULElement | null = null;
  private readonly boundUpdateVisibility = () => this.updateVisibility();

  constructor() {
    if (typeof document === "undefined") {
      return;
    }

    this.contentContextMenu = ContextMenuUtils.contentAreaContextMenu();
    if (!this.contentContextMenu) {
      console.error(
        "[ExternalBrowserLinkContextMenu] #contentAreaContextMenu not found; skip menu injection.",
      );
      return;
    }

    const marker = document?.getElementById(LINK_OPEN_MENU_ID) ?? undefined;

    try {
      render(() => this.menu(), this.contentContextMenu, {
        marker,
      });
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[ExternalBrowserLinkContextMenu] Failed to render link context menu.",
        reason,
      );
      return;
    }

    // Load browsers on init
    this.loadBrowsers();

    this.updateVisibility();

    // Observe changes to the open link menu item to sync visibility
    const openLinkElement = document?.getElementById(LINK_OPEN_MENU_ID);
    if (openLinkElement) {
      this.observer = new MutationObserver(this.boundUpdateVisibility);
      this.observer.observe(openLinkElement, {
        attributes: true,
        attributeFilter: ["hidden", "disabled"],
      });
    }

    this.contentContextMenu.addEventListener(
      "popupshowing",
      this.boundUpdateVisibility,
    );

    globalThis.addEventListener(
      "unload",
      () => {
        this.cleanup();
      },
      { once: true },
    );

    addI18nObserver(() => {
      this.updateLabels();
    });
  }

  private cleanup(): void {
    this.observer?.disconnect();
    this.observer = null;
    this.contentContextMenu?.removeEventListener(
      "popupshowing",
      this.boundUpdateVisibility,
    );
  }

  private async loadBrowsers(): Promise<void> {
    try {
      this.cachedBrowsers = await externalBrowserService.getInstalledBrowsers();
    } catch (error) {
      console.error(
        "[ExternalBrowserLinkContextMenu] Failed to load browsers:",
        error,
      );
      this.cachedBrowsers = [];
    }
  }

  private updateLabels(): void {
    const menuElem = document?.getElementById(MENU_ID);
    if (menuElem) {
      menuElem.setAttribute("label", t("externalBrowser.menu.openLinkIn"));
    }
  }

  private updateVisibility(): void {
    const menu = document?.getElementById(MENU_ID) as XULElement | null;
    const openLink = document?.getElementById(LINK_OPEN_MENU_ID) as
      | XULElement
      | null;

    if (!menu) {
      return;
    }

    if (!openLink) {
      menu.hidden = true;
      return;
    }

    const hasLink = Boolean(window.gContextMenu?.linkURL);
    const isOpenLinkHidden = Boolean(
      openLink.getAttribute("hidden") === "true" ||
        openLink.hasAttribute("hidden"),
    );

    const shouldHide = isOpenLinkHidden || !hasLink;
    menu.hidden = shouldHide;
  }

  private menu() {
    return (
      <xul:menu id={MENU_ID} label={t("externalBrowser.menu.openLinkIn")}>
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

    // Get the link URL from context menu
    const url = window.gContextMenu?.linkURL;

    if (!url) {
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
          "[ExternalBrowserLinkContextMenu] Failed to open URL:",
          result.error,
        );
      }
    } catch (error) {
      console.error(
        "[ExternalBrowserLinkContextMenu] Error opening URL:",
        error,
      );
    }
  }
}
