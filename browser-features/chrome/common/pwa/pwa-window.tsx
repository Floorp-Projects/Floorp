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
import {
  getSsbWindowTitle,
  SsbWindowContainerIndicator,
} from "./SsbWindowContainerIndicator.tsx";

export class PwaWindowSupport {
  private ssbId = createSignal<string | null>(null);
  private initialized = false;

  constructor(private pwaService: PwaService) {
    this.waitForSsbIdAndInit();
  }

  private waitForSsbIdAndInit(): void {
    const tryStart = () => {
      const ssbIdAttr = document?.documentElement?.getAttribute("taskbartab");
      if (!ssbIdAttr || this.initialized) {
        return Boolean(ssbIdAttr);
      }

      this.initialized = true;
      this.initializeWindow();
      this.setupSignals(ssbIdAttr);
      void this.initBrowser();
      return true;
    };

    if (tryStart()) {
      return;
    }

    const observer = new MutationObserver(() => {
      if (tryStart()) {
        observer.disconnect();
      }
    });

    const docElement = document?.documentElement;
    if (!docElement) {
      return;
    }

    observer.observe(docElement, {
      attributes: true,
      attributeFilter: ["taskbartab"],
    });

    globalThis.setTimeout(() => {
      observer.disconnect();
      tryStart();
    }, 500);
  }

  private async getSsb() {
    const [ssbId] = this.ssbId;
    const id = ssbId();
    if (!id) {
      return null;
    }

    const direct = await this.pwaService.getSsbObj(id);
    if (direct) {
      return direct;
    }

    const apps = await this.pwaService.getInstalledApps();
    const normalizedId = id.replace(/[{}]/g, "").toLowerCase();
    for (const manifest of Object.values(apps)) {
      const candidate = manifest.id.replace(/[{}]/g, "").toLowerCase();
      if (candidate === normalizedId) {
        return manifest;
      }
    }

    console.warn("[PwaWindowSupport] SSB not found for taskbartab id:", id);
    return null;
  }

  private async initBrowser() {
    await this.renderStyles();
    await this.setupPageActions();
    await this.setupTabs();
    await this.setupContainerVisual();
    this.disableUrlbarInteractions();
  }

  private initializeWindow(): void {
    globalThis.floorpSsbWindow = true;
    this.configureTitlebarBehavior();
    this.updateToolbarVisibility(this.shouldShowToolbar());
  }

  private setupSignals(ssbIdAttr: string): void {
    const [, setSsbId] = this.ssbId;
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

  private getUserContextIdFromTab(): number {
    try {
      const raw = globalThis.gBrowser?.selectedTab?.getAttribute(
        "usercontextid",
      );
      if (!raw) {
        return 0;
      }
      const parsed = parseInt(raw, 10);
      return Number.isFinite(parsed) && parsed > 0 ? parsed : 0;
    } catch {
      return 0;
    }
  }

  private async resolveUserContextId(): Promise<number> {
    const fromArgs = this.getUserContextIdFromArgs();
    if (fromArgs > 0) {
      return fromArgs;
    }

    const ssb = await this.getSsb();
    if (ssb?.userContextId && ssb.userContextId > 0) {
      return ssb.userContextId;
    }

    return this.getUserContextIdFromTab();
  }

  private async waitForChromeElement(
    id: string,
    attempts = 20,
  ): Promise<Element | null> {
    for (let i = 0; i < attempts; i++) {
      const element = document?.getElementById(id);
      if (element) {
        return element;
      }
      await new Promise((resolve) => globalThis.requestAnimationFrame(resolve));
    }
    return null;
  }

  private async setupContainerVisual(): Promise<void> {
    const userContextId = await this.resolveUserContextId();
    const ssb = await this.getSsb();
    console.debug("[PwaWindowSupport] setupContainerVisual", {
      userContextId,
      ssbId: this.ssbWindowId,
      ssbUserContextId: ssb?.userContextId ?? null,
      showToolbar: this.shouldShowToolbar(),
    });

    if (userContextId <= 0) {
      return;
    }

    if (ssb) {
      globalThis.document.title = getSsbWindowTitle(ssb.name, userContextId);
    }

    await this.ensureContainerIndicator(userContextId);

    createRootHMR(() => {
      createEffect(() => {
        this.shouldShowToolbar();
        this.updateToolbarVisibility(this.shouldShowToolbar());
      });
    }, import.meta.hot);
  }

  private async ensureContainerIndicator(
    userContextId: number,
  ): Promise<void> {
    document?.getElementById("ssb-container-toolbar")?.remove();

    const allIndicators = document?.querySelectorAll("#ssb-container-indicator");
    if (allIndicators && allIndicators.length > 1) {
      for (let i = 1; i < allIndicators.length; i++) {
        allIndicators[i].remove();
      }
    }

    const navBar = await this.waitForChromeElement("nav-bar", 60);
    if (!navBar) {
      console.warn(
        "[PwaWindowSupport] nav-bar not found for container indicator",
      );
      return;
    }

    const favicon = await this.waitForChromeElement("taskbar-tabs-favicon", 60);
    if (!favicon) {
      console.warn(
        "[PwaWindowSupport] taskbar-tabs-favicon not found for container indicator",
      );
      return;
    }

    const existing = document?.getElementById("ssb-container-indicator");
    if (existing) {
      return;
    }

    let marker = favicon.nextSibling instanceof Element
      ? favicon.nextSibling
      : undefined;
    if (!marker) {
      const spacer = document?.createXULElement("hbox");
      if (!spacer) {
        return;
      }
      spacer.setAttribute("class", "ssb-container-indicator-spacer");
      favicon.insertAdjacentElement("afterend", spacer);
      marker = spacer;
    }

    console.debug(
      "[PwaWindowSupport] mounting ssb-container-indicator beside taskbar-tabs-favicon",
    );
    render(
      () => <SsbWindowContainerIndicator userContextId={userContextId} />,
      navBar,
      { marker },
    );
  }

  private getUserContextIdFromArgs(): number {
    try {
      const args =
        (window as { arguments?: [unknown, nsIPropertyBag2] }).arguments;
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
      (c: { userContextId: number }) => c.userContextId === userContextId,
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
           #status-bar, #PersonalToolbar, #titlebar {
             display: none;
           }
           #nav-bar-customization-target,
           #urlbar-container,
           #nav-bar .titlebar-spacer,
           #nav-bar toolbartabstop {
             display: none !important;
           }
           #nav-bar {
             display: flex !important;
             min-height: 28px !important;
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

      const navBar = doc.getElementById("nav-bar");
      if (navBar) {
        navBar.removeAttribute("hidden");
        navBar.removeAttribute("collapsed");
        if (showToolbar) {
          navBar.removeAttribute("style");
        } else {
          navBar.setAttribute("style", "display: flex;");
        }
      }

      const titlebar = doc.getElementById("titlebar");
      if (titlebar) {
        if (showToolbar) {
          titlebar.removeAttribute("hidden");
          titlebar.removeAttribute("collapsed");
          titlebar.removeAttribute("style");
        } else {
          titlebar.setAttribute("hidden", "true");
          titlebar.setAttribute("collapsed", "true");
          titlebar.setAttribute("style", "display: none;");
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
