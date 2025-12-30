/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { _setConfig, getConfig, isEnabled, setEnabled } from "./config.ts";
import { KeyboardShortcutController } from "./controller.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";
import type { KeyboardShortcutConfig } from "./type.ts";

export class KeyboardShortcutService {
  private controllers: Map<Window, KeyboardShortcutController> = new Map();
  private lastConfigString = "";

  constructor() {
    this.initialize();

    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);
      const enabled = isEnabled();

      if (this.lastConfigString && this.lastConfigString !== configString) {
        this.destroyAllControllers();
        if (enabled) {
          this.attachToAllWindows();
        }
      }

      this.lastConfigString = configString;
    });

    createEffect(() => {
      const enabled = isEnabled();
      if (enabled) {
        this.attachToAllWindows();
      } else {
        this.destroyAllControllers();
      }
    });
  }

  private attachToAllWindows(): void {
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext() as Window;
      this.attachToWindow(win);
    }
  }

  public attachToWindow(win: Window): void {
    if (this.controllers.has(win)) {
      return;
    }

    if (isEnabled()) {
      const controller = new KeyboardShortcutController(win);
      this.controllers.set(win, controller);

      const onUnload = () => {
        controller.destroy();
        this.controllers.delete(win);
        try {
          win.removeEventListener("unload", onUnload);
        } catch (_e) {
          // Window might be already gone
        }
      };

      win.addEventListener("unload", onUnload, { once: true });
    }
  }

  private destroyAllControllers(): void {
    for (const controller of this.controllers.values()) {
      controller.destroy();
    }
    this.controllers.clear();
  }

  private initialize(): void {
    this.lastConfigString = JSON.stringify(getConfig());
    if (isEnabled()) {
      this.attachToAllWindows();
    }
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    setEnabled(value);

    if (value) {
      this.attachToAllWindows();
    } else {
      this.destroyAllControllers();
    }
  }

  public getConfig(): KeyboardShortcutConfig {
    return getConfig();
  }

  public updateConfig(newConfig: KeyboardShortcutConfig): void {
    setConfig(newConfig);

    this.destroyAllControllers();
    if (isEnabled()) {
      this.attachToAllWindows();
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
