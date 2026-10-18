/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  formatXulComboText,
  type XulCombo,
  xulComboToCanonicalCode,
} from "../common/NRKeySyncCombo.ts";
import type {
  CustomKeysModule,
  FirefoxKeyEntry,
  KeySyncChangeListener,
  KeySyncChangeResult,
} from "../common/NRKeySyncTypes.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const IS_MAC: boolean = AppConstants.platform === "macosx";

// Firefox 147+ only; absence degrades the service to read-only mirroring.
const CUSTOM_KEYS_MODULE_URI =
  "moz-src:///browser/components/customkeys/CustomKeys.sys.mjs";

const EXTENSION_KEYSET_PREFIX = "ext-keyset-id-";

/** CustomKeys.sys.mjs named export. */
interface CustomKeysModuleHolder {
  CustomKeys: CustomKeysModule;
}

/** Per-window observer bookkeeping. */
interface KeySyncWindowState {
  observers: MutationObserver[];
  onUnload: () => void;
}

/** Structural type for windows that expose openTrustedLinkIn (not in gecko d.ts). */
type BrowserWindow = Window & {
  openTrustedLinkIn?: (
    url: string,
    where: string,
    options?: object,
  ) => void;
};

function keysetKindForId(keysetId: string): "main" | "extension" {
  return keysetId.startsWith(EXTENSION_KEYSET_PREFIX) ? "extension" : "main";
}

/**
 * Build a stable synthetic id for a `<key>` element that has no DOM id.
 * The attributes are part of the identity so equivalent keys from separate
 * browser windows still share one mirror entry, while distinct shortcuts in
 * the same keyset remain separate.
 */
function syntheticKeyId(
  keysetId: string,
  index: number,
  key: Element,
): string {
  const modifiers = key.getAttribute("modifiers") ?? "";
  const keyValue = key.getAttribute("key") ?? "";
  const keycode = key.getAttribute("keycode") ?? "";
  const command = key.getAttribute("command") ?? "";
  return `id-less:${keysetId}:${index}:${modifiers}:${keyValue}:${keycode}:${command}`;
}

/**
 * Derive a fallback label from a key id, e.g. "key_newNavigator" ->
 * "New Navigator" (key_ prefix stripped, camelCase / separators split,
 * first letter of each word uppercased).
 */
function labelFromKeyId(id: string): string {
  const stripped = id.startsWith("key_") ? id.slice(4) : id;
  const words = stripped
    .replace(/([a-z0-9])([A-Z])/g, "$1 $2")
    .split(/[^a-zA-Z0-9]+/)
    .filter((word) => word.length > 0);
  if (words.length === 0) {
    return id;
  }
  return words
    .map((word) => word.charAt(0).toUpperCase() + word.slice(1))
    .join(" ");
}

/** Escape a value for use inside a double-quoted attribute selector. */
function escapeAttributeValue(value: string): string {
  return value.replace(/["\\]/g, "\\$&");
}

/** Best-effort fallback label for id-less keys. */
function fallbackLabelForKey(
  key: Element,
  keysetId: string,
): string {
  const source = key.getAttribute("command") ||
    key.getAttribute("keycode") ||
    key.getAttribute("key") ||
    keysetId;
  return labelFromKeyId(source || "unnamed-key");
}

/**
 * Mirror of the live key elements of all attached browser windows.
 * Reads are passive (MutationObserver based); writes go exclusively through
 * the CustomKeys API, which also handles persistence to customKeys.json.
 */
export class KeySyncService {
  #mirror = new Map<string, FirefoxKeyEntry>();
  #windowStates = new Map<Window, KeySyncWindowState>();
  /** Keyset elements that already have a key-change observer attached. */
  #observedKeysets = new WeakSet<Element>();
  #listeners: KeySyncChangeListener[] = [];
  #customKeys: CustomKeysModule | null = null;
  #initialized = false;

  /**
   * Idempotent bootstrap: feature-detect CustomKeys, attach to every existing
   * browser window, then track window creation/closure. A second call is a
   * no-op.
   */
  init(): void {
    if (this.#initialized) {
      return;
    }
    this.#initialized = true;

    try {
      const { CustomKeys } = ChromeUtils.importESModule(
        CUSTOM_KEYS_MODULE_URI,
      ) as CustomKeysModuleHolder;
      this.#customKeys = CustomKeys;
    } catch (_error) {
      // Firefox < 147 (or CustomKeys disabled): keep mirroring but disable writes.
      this.#customKeys = null;
    }

    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext() as Window;
      try {
        this.#attachToWindow(win);
      } catch (error) {
        console.error("[KeySyncService] attachToWindow failed:", error);
      }
    }

    const windowListener: nsIWindowMediatorListener = {
      onOpenWindow: (xulWindow: nsIAppWindow): void => {
        try {
          const domWindow = xulWindow.docShell.domWindow as Window | null;
          if (!domWindow) {
            return;
          }
          domWindow.addEventListener(
            "load",
            () => {
              if (!this.#isBrowserWindow(domWindow)) {
                return;
              }
              try {
                this.#attachToWindow(domWindow);
              } catch (error) {
                console.error("[KeySyncService] attachToWindow failed:", error);
              }
            },
            { once: true },
          );
        } catch (error) {
          console.error("[KeySyncService] onOpenWindow failed:", error);
        }
      },
      onCloseWindow: (xulWindow: nsIAppWindow): void => {
        try {
          const domWindow = xulWindow.docShell.domWindow as Window | null;
          if (!domWindow) {
            return;
          }
          this.#detachWindow(domWindow);
        } catch (error) {
          console.error("[KeySyncService] onCloseWindow failed:", error);
        }
      },
    };
    // The service is a process-lifetime singleton, so the window mediator
    // listener is intentionally never removed. Per-window observers are
    // detached via #detachWindow on window close.
    Services.wm.addListener(windowListener);
  }

  /** Whether the CustomKeys write API is usable in this build. */
  isWriteEnabled(): boolean {
    return this.#customKeys !== null;
  }

  /**
   * Snapshot of the mirrored keys: main keyset entries first, extension
   * keyset entries afterwards, each group sorted by id. The returned array
   * is a fresh copy safe for callers to retain.
   */
  getKeys(): FirefoxKeyEntry[] {
    const entries = Array.from(this.#mirror.values());
    const byId = (a: FirefoxKeyEntry, b: FirefoxKeyEntry): number =>
      a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
    const main = entries
      .filter((entry) => entry.keysetKind === "main")
      .sort(byId);
    const extension = entries
      .filter((entry) => entry.keysetKind === "extension")
      .sort(byId);
    return [...main, ...extension];
  }

  /**
   * Apply a new combo to a key through CustomKeys. Guarded for availability,
   * unknown ids, internal keys and extension keysets. An empty combo (no key
   * and no keycode) clears the shortcut. Persistence is handled by CustomKeys
   * itself; the mirror is refreshed immediately on success.
   */
  changeKey(id: string, combo: XulCombo): KeySyncChangeResult {
    const customKeys = this.#customKeys;
    if (!customKeys) {
      return {
        ok: false,
        error: "CustomKeys is not available in this build",
      };
    }
    const guard = this.#guardEditable(id);
    if (guard) {
      return guard;
    }
    const hasKey = typeof combo.key === "string" && combo.key.length > 0;
    const hasKeycode = typeof combo.keycode === "string" &&
      combo.keycode.length > 0;
    try {
      if (!hasKey && !hasKeycode) {
        customKeys.clearKey(id);
      } else {
        const payload: {
          modifiers?: string;
          key?: string;
          keycode?: string;
        } = {
          modifiers: combo.modifiers,
        };
        // key and keycode are mutually exclusive; key wins (Gecko precedence).
        if (hasKey) {
          payload.key = combo.key;
        } else {
          payload.keycode = combo.keycode;
        }
        customKeys.changeKey(id, payload);
      }
    } catch (error) {
      console.error("[KeySyncService] changeKey failed:", error);
      return {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
    this.#refreshAll();
    return { ok: true };
  }

  /** Restore the default binding of a key through CustomKeys. */
  resetKey(id: string): KeySyncChangeResult {
    const customKeys = this.#customKeys;
    if (!customKeys) {
      return {
        ok: false,
        error: "CustomKeys is not available in this build",
      };
    }
    const guard = this.#guardEditable(id);
    if (guard) {
      return guard;
    }
    try {
      customKeys.resetKey(id);
    } catch (error) {
      console.error("[KeySyncService] resetKey failed:", error);
      return {
        ok: false,
        error: error instanceof Error ? error.message : String(error),
      };
    }
    this.#refreshAll();
    return { ok: true };
  }

  /** Open Firefox's keyboard customization page in a new tab. */
  openAboutKeyboard(win: Window): void {
    const browserWin = win as BrowserWindow;
    if (typeof browserWin.openTrustedLinkIn !== "function") {
      console.error(
        "[KeySyncService] openTrustedLinkIn is unavailable on this window",
      );
      return;
    }
    browserWin.openTrustedLinkIn("about:keyboard", "tab");
  }

  addListener(fn: KeySyncChangeListener): void {
    if (!this.#listeners.includes(fn)) {
      this.#listeners.push(fn);
    }
  }

  removeListener(fn: KeySyncChangeListener): void {
    const index = this.#listeners.indexOf(fn);
    if (index >= 0) {
      this.#listeners.splice(index, 1);
    }
  }

  /** Shared write guards: unknown id, internal keys, extension keysets. */
  #guardEditable(id: string): KeySyncChangeResult | null {
    const entry = this.#mirror.get(id);
    if (!entry) {
      return { ok: false, error: "Unknown key id" };
    }
    if (entry.internal) {
      return { ok: false, error: "This key is not editable (internal)" };
    }
    if (entry.keysetKind === "extension") {
      return {
        ok: false,
        error: "Extension shortcuts cannot be edited here",
      };
    }
    return null;
  }

  #isBrowserWindow(win: Window): boolean {
    try {
      const documentElement = win.document.documentElement;
      return documentElement.getAttribute("windowtype") === "navigator:browser";
    } catch (_error) {
      return false;
    }
  }

  #attachToWindow(win: Window): void {
    if (this.#windowStates.has(win)) {
      return;
    }
    const doc = win.document;
    if (!doc) {
      return;
    }
    const observers: MutationObserver[] = [];

    const attributeCallback = (): void => {
      try {
        this.#refreshFromWindow(win);
      } catch (error) {
        console.error("[KeySyncService] attribute refresh failed:", error);
      }
    };
    // CustomKeys refreshKeysets detaches and re-appends keysets; childList
    // mutations let us notice re-attached and newly created keysets.
    const childListCallback = (): void => {
      try {
        this.#observeKeysets(win, observers, attributeCallback);
        this.#refreshFromWindow(win);
      } catch (error) {
        console.error("[KeySyncService] childList refresh failed:", error);
      }
    };

    this.#observeKeysets(win, observers, attributeCallback);
    const documentObserver = new win.MutationObserver(childListCallback);
    documentObserver.observe(doc.documentElement, { childList: true });
    observers.push(documentObserver);

    const onUnload = (): void => {
      this.#detachWindow(win);
    };
    win.addEventListener("unload", onUnload, { once: true });

    this.#windowStates.set(win, { observers, onUnload });

    // Initial scan.
    this.#refreshFromWindow(win);
  }

  /** Attach key-change observers to every keyset that is not observed yet. */
  #observeKeysets(
    win: Window,
    observers: MutationObserver[],
    attributeCallback: MutationCallback,
  ): void {
    const doc = win.document;
    if (!doc) {
      return;
    }
    for (const keyset of doc.querySelectorAll("keyset[id]")) {
      if (this.#observedKeysets.has(keyset)) {
        continue;
      }
      this.#observedKeysets.add(keyset);
      const observer = new win.MutationObserver(attributeCallback);
      observer.observe(keyset, {
        attributes: true,
        childList: true,
        attributeFilter: ["key", "keycode", "modifiers"],
        // Key attribute and child-list changes happen on the <key> children,
        // not the keyset itself.
        subtree: true,
      });
      observers.push(observer);
    }
  }

  #detachWindow(win: Window): void {
    const state = this.#windowStates.get(win);
    if (!state) {
      return;
    }
    for (const observer of state.observers) {
      try {
        observer.disconnect();
      } catch (_error) {
        // The window/document may already be gone.
      }
    }
    try {
      win.removeEventListener("unload", state.onUnload);
    } catch (_error) {
      // The window may already be gone.
    }
    this.#windowStates.delete(win);
  }

  /**
   * Re-read every key element of the window and reconcile it with the
   * mirror (same id across windows is assumed identical, last write wins).
   * Listeners are notified only when the resulting snapshot actually changed.
   */
  #refreshFromWindow(win: Window): void {
    const doc = win.document;
    if (!doc) {
      return;
    }
    const before = JSON.stringify(this.getKeys());
    const currentIds = new Set<string>();
    for (const keyEl of doc.querySelectorAll("keyset[id] > key")) {
      const keyset = keyEl.parentElement;
      if (!keyset || !keyset.id) {
        continue;
      }
      // Key elements without a DOM id cannot be addressed by CustomKeys, but
      // they are still real Firefox shortcuts and must remain visible as
      // read-only rows.
      const syntheticId = keyEl.id
        ? keyEl.id
        : syntheticKeyId(
          keyset.id,
          Array.from(keyset.children).indexOf(keyEl),
          keyEl,
        );
      const entry = this.#buildEntry(
        keyEl,
        doc,
        keysetKindForId(keyset.id),
        syntheticId,
      );
      currentIds.add(entry.id);
      this.#mirror.set(entry.id, entry);
    }

    // A window's removal must not discard a key still provided by another
    // attached window. Check the live documents instead of assuming that
    // mirror ids are uniquely owned.
    const otherWindowIds = new Set<string>();
    for (const otherWin of this.#windowStates.keys()) {
      if (otherWin === win) {
        continue;
      }
      try {
        for (const keyEl of otherWin.document.querySelectorAll(
          "keyset[id] > key",
        )) {
          const keyset = keyEl.parentElement;
          if (!keyset || !keyset.id) {
            continue;
          }
          otherWindowIds.add(
            keyEl.id
              ? keyEl.id
              : syntheticKeyId(
                keyset.id,
                Array.from(keyset.children).indexOf(keyEl),
                keyEl,
              ),
          );
        }
      } catch (_error) {
        // The other window may be going away.
      }
    }

    for (const id of Array.from(this.#mirror.keys())) {
      if (!currentIds.has(id) && !otherWindowIds.has(id)) {
        this.#mirror.delete(id);
      }
    }

    const after = JSON.stringify(this.getKeys());
    if (before !== after) {
      this.#notifyListeners();
    }
  }

  #refreshAll(): void {
    for (const win of Array.from(this.#windowStates.keys())) {
      try {
        this.#refreshFromWindow(win);
      } catch (error) {
        console.error("[KeySyncService] refreshAll failed:", error);
      }
    }
  }

  #notifyListeners(): void {
    const keys = this.getKeys();
    for (const listener of Array.from(this.#listeners)) {
      try {
        listener(keys);
      } catch (error) {
        console.error("[KeySyncService] listener failed:", error);
      }
    }
  }

  #buildEntry(
    keyEl: Element,
    doc: Document,
    keysetKind: "main" | "extension",
    id: string,
  ): FirefoxKeyEntry {
    const modifiers = keyEl.getAttribute("modifiers") ?? "";
    const key = keyEl.getAttribute("key") ?? "";
    const keycode = keyEl.getAttribute("keycode") ?? "";
    const combo: XulCombo = {
      modifiers,
      key: key || undefined,
      keycode: keycode || undefined,
    };
    return {
      id,
      keysetKind,
      modifiers,
      key,
      keycode,
      command: keyEl.getAttribute("command") ?? "",
      internal: keyEl.id === "" || keyEl.getAttribute("internal") === "true",
      reserved: keyEl.getAttribute("reserved") === "true",
      customized: this.#customKeys
        ? this.#customKeys.getDefaultKey(id) != null
        : false,
      label: this.#resolveLabel(
        doc,
        keyEl,
        keyEl.id
          ? labelFromKeyId(id)
          : fallbackLabelForKey(keyEl, keyEl.parentElement?.id ?? ""),
      ),
      shortcutText: formatXulComboText(combo, IS_MAC),
      canonicalCode: xulComboToCanonicalCode(combo, IS_MAC),
    };
  }

  /**
   * Best-effort label resolution: command element's label, then a matching
   * menuitem's label, then a generated label derived from the key id.
   */
  #resolveLabel(
    doc: Document,
    keyEl: Element,
    fallbackLabel: string,
  ): string {
    const commandId = keyEl.getAttribute("command");
    if (commandId) {
      const commandLabel =
        doc.getElementById(commandId)?.getAttribute("label") ?? "";
      if (commandLabel) {
        return commandLabel;
      }
    }
    const keyValue = keyEl.getAttribute("key");
    if (keyValue) {
      const menuitem = doc.querySelector(
        `menuitem[key="${escapeAttributeValue(keyValue)}"]`,
      );
      const menuLabel = menuitem?.getAttribute("label") ?? "";
      if (menuLabel) {
        return menuLabel;
      }
    }
    return fallbackLabel;
  }
}

export const keySyncService = new KeySyncService();
