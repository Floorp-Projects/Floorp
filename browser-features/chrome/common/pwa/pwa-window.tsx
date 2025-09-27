/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { createSignal } from "solid-js";
import { config } from "./config.ts";
import PwaWindowStyle from "./pwa-window-style.css?inline";
import type { PwaService } from "./pwaService.ts";
const { PWA_WINDOW_NAME } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
);

export class PwaWindowSupport {
  private ssbId = createSignal<string | null>(null);

  private async getSsb() {
    const [ssbId] = this.ssbId;
    return ssbId ? await this.pwaService.getSsbObj(ssbId() as string) : null;
  }

  constructor(private pwaService: PwaService) {
    if (!window.name.startsWith(PWA_WINDOW_NAME)) {
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
  }

  private initializeWindow(): void {
    const mainWindow = document?.getElementById("main-window");
    mainWindow?.setAttribute("windowtype", "navigator:ssb-window");
    window.floorpSsbWindow = true;
  }

  private setupSignals(): void {
    const [, setSsbId] = this.ssbId;
    setSsbId(window.name);
  }

  private setupPageActions(): void {
    const identityBox = document?.getElementById("identity-box");
    const pageActionBox = document?.getElementById("page-action-buttons");
    if (identityBox && pageActionBox) {
      identityBox.after(pageActionBox);
    }
  }

  private setupTabs(): void {
    window.gBrowser.tabs.forEach((tab: XULElement) => {
      tab.setAttribute("floorpSSB", "true");
    });
  }

  private renderStyles(): void {
    createRootHMR(() => {
      render(() => this.createStyleElement(), document?.head);
    }, import.meta.hot);
  }

  private createStyleElement() {
    const showToolbar = config().showToolbar !== false;

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

  public get ssbWindowId(): string | null {
    const [ssbId] = this.ssbId;
    return ssbId();
  }

  public async getSsbObj(id: string) {
    return await this.pwaService.getSsbObj(id);
  }
}
