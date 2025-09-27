/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { _setConfig, getConfig, isEnabled, setEnabled } from "./config.ts";
import { KeyboardShortcutController } from "./controller.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";
import type { KeyboardShortcutConfig } from "./type.ts";

let globalController: KeyboardShortcutController | null = null;

export class KeyboardShortcutService {
  private controller: KeyboardShortcutController | null = null;
  private lastConfigString = "";

  constructor() {
    if (globalController) {
      globalController.destroy();
      globalController = null;
    }

    this.initialize();

    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);

      if (this.lastConfigString && this.lastConfigString !== configString) {
        this.destroyController();
        if (isEnabled()) {
          this.createController();
        }
      }

      this.lastConfigString = configString;
    });
  }

  private initialize(): void {
    this.lastConfigString = JSON.stringify(getConfig());
    if (isEnabled()) {
      this.createController();
    }
  }

  private createController(): void {
    this.destroyController();

    this.controller = new KeyboardShortcutController();
    globalController = this.controller;
  }

  private destroyController(): void {
    if (this.controller) {
      this.controller.destroy();
      this.controller = null;

      if (globalController === this.controller) {
        globalController = null;
      }
    }
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    const currentState = isEnabled();

    setEnabled(value);

    if (value && !currentState) {
      this.createController();
    } else if (!value && currentState) {
      this.destroyController();
    }
  }

  public getConfig(): KeyboardShortcutConfig {
    return getConfig();
  }

  public updateConfig(newConfig: KeyboardShortcutConfig): void {
    setConfig(newConfig);

    if (isEnabled()) {
      this.destroyController();
      this.createController();
    }
  }
}

function setConfig(config: KeyboardShortcutConfig) {
  _setConfig(config);
}

function createKeyboardShortcutService() {
  return new KeyboardShortcutService();
}

export const keyboardShortcutService = createRootHMR(
  createKeyboardShortcutService,
  import.meta.hot,
);
