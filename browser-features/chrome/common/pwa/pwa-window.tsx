/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { createEffect, createSignal } from "solid-js";
import { config } from "./config.ts";
import PwaWindowStyle from "./pwa-window-style.css?inline";
import type { PwaService } from "./pwaService.ts";
import type { FloorpChromeWindow } from "./type.ts";

export class PwaWindowSupport {
  private ssbId = createSignal<string | null>(null);

  private async getSsb() {
    const [ssbId] = this.ssbId;
    return ssbId ? await this.pwaService.getSsbObj(ssbId() as string) : null;
  }

  constructor(private pwaService: PwaService) {
    // Check if this is a PWA window using documentElement attribute
    // Note: We use "taskbartab" instead of "ssbid" because browser-init.js
    // only sets taskbartab attribute from extraOptions. PWA windows set both
    // with the same ID value in SsbCommandLineHandler.
    const ssbIdAttr = document?.documentElement?.getAttribute("taskbartab");
    if (!ssbIdAttr) {
      return;
    }

    this.initializeWindow();
    this.setupSignals();
    this.initBrowser();
  }

  private async initBrowser() {
    await this.renderStyles();
    await this.setupPageActions();
    await this.setupTabs();
    this.disableUrlbarInteractions();
  }

  private initializeWindow(): void {
    globalThis.floorpSsbWindow = true;
    this.configureTitlebarBehavior();
    this.updateToolbarVisibility(this.shouldShowToolbar());
  }

  private setupSignals(): void {
    const [, setSsbId] = this.ssbId;
    // Read SSB ID from documentElement attribute (using taskbartab as set by browser-init.js)
    const ssbIdAttr = document?.documentElement?.getAttribute("taskbartab") ??
      null;
    setSsbId(ssbIdAttr);
  }

  private setupPageActions(): void {
    const identityBox = document?.getElementById("identity-box");
    const pageActionBox = document?.getElementById("page-action-buttons");
    if (identityBox && pageActionBox) {
      identityBox.after(pageActionBox);
    }
  }

  private setupTabs(): void {
    globalThis.gBrowser.tabs.forEach((tab: XULElement) => {
      tab.setAttribute("floorpSSB", "true");
    });

    const userContextId = this.getUserContextIdFromArgs();
    if (userContextId > 0) {
      this.applyUserContext(userContextId);
    }
  }

  private getUserContextIdFromArgs(): number {
    try {
      const args = (window as { arguments?: [unknown, nsIPropertyBag2] }).arguments;
      if (args?.[1]) {
        const extraOptions = args[1];
        if (extraOptions.hasKey("userContextId")) {
          const val = extraOptions.getPropertyAsAString("userContextId");
          return parseInt(val, 10) || 0;
        }
      }
    } catch {
      // If we can't read the args, default to no container
    }
    return 0;
  }

  private applyUserContext(userContextId: number): void {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );
    const containers = ContextualIdentityService.getPublicIdentities();
    const exists = containers.some(
      // deno-lint-ignore no-explicit-any
      (c: any) => c.userContextId === userContextId,
    );

    if (!exists) {
      console.warn(
        `[PwaWindowSupport] Container ${userContextId} no longer exists, falling back to no container`,
      );
      return;
    }

    const tab = globalThis.gBrowser.selectedTab;
    if (tab.getAttribute("usercontextid") === String(userContextId)) {
      return;
    }

    const url = globalThis.gBrowser.selectedBrowser?.currentURI?.spec ??
      "about:blank";

    const triggeringPrincipal = Services.scriptSecurityManager
      .createNullPrincipal({
        userContextId,
      }) as nsIPrincipal;

    const newTab = globalThis.gBrowser.addTrustedTab(url, {
      userContextId,
      triggeringPrincipal,
    });

    globalThis.gBrowser.selectedTab = newTab;
    globalThis.gBrowser.removeTab(tab);
  }

  private renderStyles(): void {
    createRootHMR(() => {
      render(() => this.createStyleElement(), document?.head);
    }, import.meta.hot);
  }

  private configureTitlebarBehavior(): void {
    try {
      const chromeWindow = window as FloorpChromeWindow;
      const customTitlebar = chromeWindow.CustomTitlebar;
      if (!customTitlebar?.allowedBy) {
        return;
      }

      if (!customTitlebar.__floorpSsbPatched) {
        const originalAllowedBy = customTitlebar.allowedBy.bind(customTitlebar);
        customTitlebar.allowedBy = (condition: string, allow: boolean) => {
          if (condition === "non-popup") {
            originalAllowedBy(
              condition,
              this.shouldUseCustomTitlebar(),
            );
            return;
          }
          originalAllowedBy(condition, allow);
        };
        customTitlebar.__floorpSsbPatched = true;
      }

      createRootHMR(() => {
        createEffect(() => {
          const showToolbar = this.shouldShowToolbar();
          // When the toolbar is hidden we want the window to use the native titlebar.
          customTitlebar.allowedBy("non-popup", this.shouldUseCustomTitlebar());
          this.updateToolbarVisibility(showToolbar);
        });
      }, import.meta.hot);
    } catch (error) {
      console.error(
        "[PwaWindowSupport] Failed to configure titlebar behavior:",
        error,
      );
    }
  }

  private shouldShowToolbar(): boolean {
    return config().showToolbar !== false;
  }

  private shouldUseCustomTitlebar(): boolean {
    return this.shouldShowToolbar();
  }

  private createStyleElement() {
    const showToolbar = this.shouldShowToolbar();

    return (
      <style>
        {PwaWindowStyle}
        {!showToolbar
          ? `
           #nav-bar, #status-bar, #PersonalToolbar, #titlebar {
             display: none;
           }
         `
          : ""}
      </style>
    );
  }

  private disableUrlbarInteractions(): void {
    try {
      const doc = globalThis.document;
      if (!doc) {
        return;
      }

      const urlbarInput = doc.getElementById(
        "urlbar-input",
      ) as HTMLInputElement | null;
      if (urlbarInput) {
        urlbarInput.readOnly = true;
        urlbarInput.setAttribute("aria-readonly", "true");
      }
    } catch (error) {
      console.error(
        "[PwaWindowSupport] Failed to disable urlbar interactions:",
        error,
      );
    }
  }

  private updateToolbarVisibility(showToolbar: boolean): void {
    try {
      const doc = globalThis.document;
      if (!doc) {
        return;
      }

      const elements = [
        doc.getElementById("nav-bar"),
        doc.getElementById("status-bar"),
        doc.getElementById("PersonalToolbar"),
      ];

      for (const element of elements) {
        if (!element) {
          continue;
        }

        element.removeAttribute("hidden");
        element.removeAttribute("collapsed");
        element.removeAttribute("style");

        if (!showToolbar) {
          element.setAttribute("hidden", "true");
          element.setAttribute("collapsed", "true");
          element.setAttribute("style", "display: none;");
        }
      }
    } catch (error) {
      console.error(
        "[PwaWindowSupport] Failed to update toolbar visibility:",
        error,
      );
    }
  }

  public get ssbWindowId(): string | null {
    const [ssbId] = this.ssbId;
    return ssbId();
  }

  public async getSsbObj(id: string) {
    return await this.pwaService.getSsbObj(id);
  }
}
