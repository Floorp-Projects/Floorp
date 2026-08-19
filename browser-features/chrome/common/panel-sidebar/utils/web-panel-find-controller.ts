/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { WebPanelBrowserElement } from "./web-panel-browser.ts";

export const WEB_PANEL_FINDBAR_ID = "floorp-webpanel-findbar";

const WEB_PANEL_FIND_COMMAND_IDS = [
  "cmd_find",
  "cmd_findAgain",
  "cmd_findPrevious",
  "cmd_findSelection",
] as const;

type WebPanelFindCommandId = (typeof WEB_PANEL_FIND_COMMAND_IDS)[number];

type WebPanelFindbarElement = XULElement & {
  browser: WebPanelBrowserElement | null;
  destroy?: () => void;
  onFindCommand: () => void;
  onFindAgainCommand: (findPrevious: boolean) => void;
  onFindSelectionCommand?: () => void;
};

export interface WebPanelFindDocument {
  getElementById(id: string): Element | null;
  createXULElement(name: string): Element;
}

export interface WebPanelFindWindow {
  addEventListener(
    type: string,
    listener: EventListener,
  ): void;
  removeEventListener(
    type: string,
    listener: EventListener,
  ): void;
  requestAnimationFrame(callback: FrameRequestCallback): number;
}

function isWebPanelFindCommandId(id: string): id is WebPanelFindCommandId {
  return WEB_PANEL_FIND_COMMAND_IDS.includes(id as WebPanelFindCommandId);
}

/**
 * Owns the findbar used by the browser.xhtml instance embedded in a web panel.
 *
 * The normal browser command handler asks gBrowser for a tab findbar. A web
 * panel intentionally has no gBrowser, so these listeners route only the find
 * commands to a findbar bound directly to the panel's content browser.
 */
export class WebPanelFindController {
  private readonly commandElements = new Set<Element>();
  private findbar: WebPanelFindbarElement | null = null;
  private pendingFindbar: Promise<WebPanelFindbarElement | null> | null = null;
  private pendingFindbarElement: WebPanelFindbarElement | null = null;
  private readonly removedFindbars = new WeakSet<WebPanelFindbarElement>();
  private initialized = false;
  private destroyed = false;

  constructor(
    private readonly browser: WebPanelBrowserElement,
    private readonly panelDocument: WebPanelFindDocument = document,
    private readonly panelWindow: WebPanelFindWindow = window,
  ) {}

  private readonly handleCommand = (event: Event): void => {
    const commandId = (event.currentTarget as Element | null)?.id ?? "";
    if (!isWebPanelFindCommandId(commandId)) {
      return;
    }

    event.preventDefault();
    event.stopImmediatePropagation();
    void this.executeCommand(commandId);
  };

  private readonly handleUnload = (): void => {
    this.destroy();
  };

  init(): void {
    if (this.initialized || this.destroyed) {
      return;
    }

    for (const commandId of WEB_PANEL_FIND_COMMAND_IDS) {
      const commandElement = this.panelDocument.getElementById(commandId);
      if (!commandElement) {
        continue;
      }

      commandElement.addEventListener("command", this.handleCommand, true);
      this.commandElements.add(commandElement);
    }

    this.panelWindow.addEventListener("unload", this.handleUnload);
    this.initialized = true;
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }

    this.destroyed = true;
    this.initialized = false;

    for (const commandElement of this.commandElements) {
      commandElement.removeEventListener("command", this.handleCommand, true);
    }
    this.commandElements.clear();
    this.panelWindow.removeEventListener("unload", this.handleUnload);

    if (this.findbar) {
      this.removeFindbar(this.findbar);
    }
    if (this.pendingFindbarElement) {
      this.removeFindbar(this.pendingFindbarElement);
    }

    this.findbar = null;
    this.pendingFindbarElement = null;
  }

  private async executeCommand(
    commandId: WebPanelFindCommandId,
  ): Promise<void> {
    try {
      const findbar = await this.getFindbar();
      if (!findbar) {
        return;
      }

      switch (commandId) {
        case "cmd_find":
          findbar.onFindCommand();
          break;
        case "cmd_findAgain":
          findbar.onFindAgainCommand(false);
          break;
        case "cmd_findPrevious":
          findbar.onFindAgainCommand(true);
          break;
        case "cmd_findSelection":
          findbar.onFindSelectionCommand?.();
          break;
      }
    } catch (error) {
      console.error("[WebPanelFindController] Find command failed:", error);
    }
  }

  private async getFindbar(): Promise<WebPanelFindbarElement | null> {
    if (this.destroyed) {
      return null;
    }
    if (this.findbar) {
      return this.findbar;
    }
    if (this.pendingFindbar) {
      return await this.pendingFindbar;
    }

    const pendingFindbar = this.createFindbar();
    this.pendingFindbar = pendingFindbar;
    try {
      return await pendingFindbar;
    } finally {
      if (this.pendingFindbar === pendingFindbar) {
        this.pendingFindbar = null;
      }
    }
  }

  private async createFindbar(): Promise<WebPanelFindbarElement | null> {
    const existing = this.panelDocument.getElementById(WEB_PANEL_FINDBAR_ID) as
      | WebPanelFindbarElement
      | null;
    if (existing) {
      existing.browser = this.browser;
      this.findbar = existing;
      return existing;
    }

    const findbar = this.panelDocument.createXULElement(
      "findbar",
    ) as WebPanelFindbarElement;
    findbar.id = WEB_PANEL_FINDBAR_ID;
    this.pendingFindbarElement = findbar;

    try {
      const inserted = this.browser.insertAdjacentElement("afterend", findbar);
      if (!inserted) {
        throw new Error("Unable to insert the web panel findbar");
      }

      await new Promise<void>((resolve) => {
        this.panelWindow.requestAnimationFrame(() => resolve());
      });

      if (this.destroyed || !this.browser.isConnected || !findbar.isConnected) {
        this.removeFindbar(findbar);
        return null;
      }

      findbar.browser = this.browser;
      this.findbar = findbar;
      this.pendingFindbarElement = null;
      return findbar;
    } catch (error) {
      this.removeFindbar(findbar);
      throw error;
    } finally {
      if (this.pendingFindbarElement === findbar && this.destroyed) {
        this.pendingFindbarElement = null;
      }
    }
  }

  private removeFindbar(findbar: WebPanelFindbarElement): void {
    if (this.removedFindbars.has(findbar)) {
      return;
    }
    this.removedFindbars.add(findbar);

    try {
      findbar.destroy?.();
    } catch (error) {
      console.error("[WebPanelFindController] Findbar cleanup failed:", error);
    }
    findbar.remove();

    if (this.findbar === findbar) {
      this.findbar = null;
    }
    if (this.pendingFindbarElement === findbar) {
      this.pendingFindbarElement = null;
    }
  }
}
