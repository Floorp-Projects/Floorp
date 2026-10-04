/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { _config, getConfig, isEnabled, setEnabled } from "./config.ts";
import { KeyboardShortcutController } from "./controller.ts";
import { createRootHMR } from "#features-chrome/utils/base";
import { addDisposer, createRoot, rootEffect } from "@nora/preact-xul/lifetime";
import type { KeyboardShortcutConfig } from "./type.ts";
import type { KeyboardShortcutFocusStoreReader } from "./editable-focus.ts";

const FOCUS_PARENT_MODULE =
  "resource://noraneko/actors/NRKeyboardShortcutFocusParent.sys.mjs";

function loadRemoteFocusStore(): KeyboardShortcutFocusStoreReader | null {
  try {
    const actorModule = ChromeUtils.importESModule(FOCUS_PARENT_MODULE) as {
      nrKeyboardShortcutFocusStore?: KeyboardShortcutFocusStoreReader;
    };
    return actorModule.nrKeyboardShortcutFocusStore ?? null;
  } catch (_error) {
    // Missing remote state deliberately preserves the pre-guard behavior.
    return null;
  }
}

export class KeyboardShortcutService {
  private controller: KeyboardShortcutController | null = null;
  private disposed = true;
  private disposeRoot: (() => void) | null = null;
  private lastConfigString = "";
  private readonly remoteFocusStore: KeyboardShortcutFocusStoreReader | null;

  constructor(
    remoteFocusStore = loadRemoteFocusStore(),
    private readonly targetWindow: Window = window,
  ) {
    this.remoteFocusStore = remoteFocusStore;
    if (targetWindow.closed) return;
    this.disposed = false;
    this.disposeRoot = createRoot((dispose) => {
      targetWindow.addEventListener("unload", dispose, { once: true });
      addDisposer(() => {
        this.disposed = true;
        targetWindow.removeEventListener("unload", dispose);
        this.destroyController();
      });
      rootEffect(() => {
        const configString = JSON.stringify(getConfig());
        const enabled = isEnabled();
        if (this.disposed || targetWindow.closed) return;
        if (this.lastConfigString && this.lastConfigString !== configString) {
          this.destroyController();
        }
        this.lastConfigString = configString;
        if (enabled) this.attachToWindow(targetWindow);
        else this.destroyController();
      });
      return dispose;
    });
  }

  public attachToWindow(win: Window): void {
    if (
      this.disposed || win !== this.targetWindow || win.closed ||
      this.controller || !isEnabled()
    ) return;
    this.controller = new KeyboardShortcutController(
      win,
      this.remoteFocusStore,
    );
    // Diagnostic only: another window's realm must not reserve this window.
    (win as Window & { __keyboardShortcutControllerAttached?: boolean })
      .__keyboardShortcutControllerAttached = true;
  }

  private destroyController(): void {
    if (!this.controller) return;
    const controller = this.controller;
    this.controller = null;
    try {
      controller.destroy();
    } finally {
      delete (this.targetWindow as Window & {
        __keyboardShortcutControllerAttached?: boolean;
      }).__keyboardShortcutControllerAttached;
    }
  }

  public destroy(): void {
    this.disposeRoot?.();
    this.disposeRoot = null;
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    if (this.disposed) return;
    setEnabled(value);
  }

  public getConfig(): KeyboardShortcutConfig {
    return getConfig();
  }

  public updateConfig(newConfig: KeyboardShortcutConfig): void {
    if (this.disposed) return;
    setConfig(newConfig);
  }
}

function setConfig(config: KeyboardShortcutConfig) {
  _config.value = config;
}

function createKeyboardShortcutService() {
  return new KeyboardShortcutService();
}

export const keyboardShortcutService = createRootHMR(
  createKeyboardShortcutService,
  import.meta.hot,
);
