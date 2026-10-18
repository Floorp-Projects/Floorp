/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  FirefoxKeyEntry,
  KeySyncChangeListener,
  KeySyncChangeResult,
} from "../common/NRKeySyncTypes.ts";
import type { XulCombo } from "../common/NRKeySyncCombo.ts";

/** Structural type of the key sync module loaded via resource URI. */
interface KeySyncServiceModule {
  keySyncService: {
    init(): void;
    getKeys(): FirefoxKeyEntry[];
    isWriteEnabled(): boolean;
    changeKey(id: string, combo: XulCombo): KeySyncChangeResult;
    resetKey(id: string): KeySyncChangeResult;
    openAboutKeyboard(win: Window): void;
    addListener(listener: KeySyncChangeListener): void;
    removeListener(listener: KeySyncChangeListener): void;
  };
}

const { keySyncService } = ChromeUtils.importESModule(
  "resource://noraneko/modules/NRKeySyncService.sys.mjs",
) as KeySyncServiceModule;

/**
 * Validate a XUL combo coming from the settings page. `modifiers` is
 * required by XulCombo but tolerated as absent (""), while `key` and
 * `keycode` must be strings when present. Returns null for non-object
 * payloads or non-string combo fields.
 */
function parseCombo(value: unknown): XulCombo | null {
  if (typeof value !== "object" || value === null) {
    return null;
  }
  const combo = value as {
    modifiers?: unknown;
    key?: unknown;
    keycode?: unknown;
  };
  if (
    combo.modifiers !== undefined && typeof combo.modifiers !== "string" ||
    combo.key !== undefined && typeof combo.key !== "string" ||
    combo.keycode !== undefined && typeof combo.keycode !== "string"
  ) {
    return null;
  }
  const modifiers = typeof combo.modifiers === "string" ? combo.modifiers : "";
  const key = typeof combo.key === "string" && combo.key.length > 0
    ? combo.key
    : undefined;
  const keycode = typeof combo.keycode === "string" && combo.keycode.length > 0
    ? combo.keycode
    : undefined;
  return { modifiers, key, keycode };
}

const INVALID_ARGUMENTS: KeySyncChangeResult = {
  ok: false,
  error: "Invalid arguments",
};

export class NRKeySyncParent extends JSWindowActorParent {
  /** Push listener registered with the key sync service, null when detached. */
  private keyListener: KeySyncChangeListener | null = null;

  constructor() {
    super();
  }

  /**
   * Idempotently wire the mirror-change push channel: the first message from
   * the page registers a service listener that forwards fresh snapshots via
   * "NRKeySync:KeysChanged". didDestroy removes it again.
   */
  private registerKeyListenerOnce(): void {
    if (this.keyListener) {
      return;
    }
    const listener: KeySyncChangeListener = (keys) => {
      try {
        this.sendAsyncMessage("NRKeySync:KeysChanged", keys);
      } catch (error) {
        console.error("[NRKeySync] Failed to push the key snapshot", error);
      }
    };
    this.keyListener = listener;
    keySyncService.addListener(listener);
  }

  receiveMessage(message: { name: string; data?: unknown }): unknown {
    // init() is idempotent; guarantee the mirror is populated even when the
    // keyboard-shortcut feature has not run yet.
    try {
      keySyncService.init();
    } catch (error) {
      console.error("[NRKeySync] keySyncService.init failed", error);
    }
    this.registerKeyListenerOnce();

    const data = message.data as Record<string, unknown> | undefined;
    switch (message.name) {
      case "getFirefoxKeys":
        return keySyncService.getKeys();
      case "isKeySyncWriteEnabled":
        return keySyncService.isWriteEnabled();
      case "changeFirefoxKey": {
        const id = data && typeof data.id === "string" ? data.id : null;
        const combo = data ? parseCombo(data.combo) : null;
        if (!id || !combo) {
          return INVALID_ARGUMENTS;
        }
        return keySyncService.changeKey(id, combo);
      }
      case "resetFirefoxKey": {
        const id = data && typeof data.id === "string" ? data.id : null;
        if (!id) {
          return INVALID_ARGUMENTS;
        }
        return keySyncService.resetKey(id);
      }
      case "openAboutKeyboard": {
        // Same window acquisition as NRSettingsParent's openExternalLink.
        const browser = this.browsingContext?.top?.embedderElement;
        const win = browser?.ownerGlobal;
        if (!win) {
          console.error(
            "[NRKeySync] No browser window available to open about:keyboard",
          );
          break;
        }
        keySyncService.openAboutKeyboard(win);
        break;
      }
    }
  }

  didDestroy(): void {
    if (this.keyListener) {
      keySyncService.removeListener(this.keyListener);
      this.keyListener = null;
    }
  }
}
