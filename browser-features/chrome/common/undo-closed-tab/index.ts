/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "../../utils/base.ts";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { StyleElement } from "./styleElem.tsx";
import { BrowserActionUtils } from "../../utils/browser-action.tsx";
import i18next from "i18next";
import { createSignal, onCleanup } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

declare global {
  interface Window {
    undoCloseTab: () => void;
  }
}

type UndoClosedTabTexts = {
  buttonLabel: string;
  tooltipText: string;
};

const defaultTexts: UndoClosedTabTexts = {
  buttonLabel: "Undo Closed Tab",
  tooltipText: "Reopen the last closed tab (Ctrl+Shift+T)",
};

const BROWSER_WINDOW_TYPE = "navigator:browser";

/** Fallback tooltip shortcut used when the live key cannot be resolved. */
const FALLBACK_SHORTCUT = "(Ctrl+Shift+T)";
/** `command` attribute of the Firefox "Undo Close Tab" key element. */
const UNDO_CLOSE_TAB_COMMAND = "History:UndoCloseTab";
/** Case-insensitive marker searched in mirrored key ids. */
const UNDO_CLOSE_TAB_ID_MARKER = "undoclosetab";

/** Structural type of a mirrored `<key>` entry (NRKeySyncService). */
interface MirrorKeyEntry {
  id: string;
  command: string;
  shortcutText: string;
}

/** Structural type of the key sync module loaded via resource URI. */
interface KeySyncServiceModule {
  keySyncService: {
    getKeys(): MirrorKeyEntry[];
    addListener(fn: (keys: MirrorKeyEntry[]) => void): void;
    removeListener(fn: (keys: MirrorKeyEntry[]) => void): void;
  };
}

let keySyncServiceCache: KeySyncServiceModule["keySyncService"] | null | undefined;

function getKeySyncService(): KeySyncServiceModule["keySyncService"] | null {
  if (keySyncServiceCache !== undefined) {
    return keySyncServiceCache;
  }
  try {
    const keySyncModule = ChromeUtils.importESModule(
      "resource://noraneko/modules/NRKeySyncService.sys.mjs",
    ) as KeySyncServiceModule;
    keySyncServiceCache = keySyncModule.keySyncService;
  } catch (error) {
    console.error("[undo-closed-tab] KeySyncService is unavailable:", error);
    keySyncServiceCache = null;
  }
  return keySyncServiceCache;
}

function formatShortcutText(shortcutText: string): string {
  return shortcutText.length > 0 ? `(${shortcutText})` : FALLBACK_SHORTCUT;
}

/** Resolve the "Undo Close Tab" shortcut from the given mirror snapshot. */
function resolveShortcutFromKeys(keys: MirrorKeyEntry[]): string {
  const entry = keys.find(
    (key) =>
      key.command === UNDO_CLOSE_TAB_COMMAND ||
      key.id.toLowerCase().includes(UNDO_CLOSE_TAB_ID_MARKER),
  );
  return entry ? formatShortcutText(entry.shortcutText) : FALLBACK_SHORTCUT;
}

/** Resolve the tooltip shortcut from the live key mirror (literal fallback). */
function resolveUndoCloseTabShortcut(): string {
  const keySyncService = getKeySyncService();
  if (!keySyncService) {
    return FALLBACK_SHORTCUT;
  }
  try {
    return resolveShortcutFromKeys(keySyncService.getKeys());
  } catch (error) {
    console.error(
      "[undo-closed-tab] Failed to resolve shortcut from key mirror:",
      error,
    );
    return FALLBACK_SHORTCUT;
  }
}

function triggerUndoClosedTabForActiveWindow(): void {
  try {
    const browserWindow = Services.wm.getMostRecentWindow(
      BROWSER_WINDOW_TYPE,
    ) as Window | null;

    (
      globalThis as unknown as {
        SessionWindowUI: { undoCloseTab: (w: Window | null) => void };
      }
    ).SessionWindowUI.undoCloseTab(browserWindow);
  } catch (error) {
    console.error("[undo-closed-tab] Failed to trigger undoCloseTab:", error);
  }
}

@noraComponent(import.meta.hot)
export default class UndoClosedTab extends NoraComponentBase {
  init(): void {
    BrowserActionUtils.createToolbarClickActionButton(
      "undo-closed-tab",
      null,
      triggerUndoClosedTabForActiveWindow,
      StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      3,
      (aNode: XULElement) => {
        const tooltip = document?.createXULElement("tooltip") as XULElement;
        tooltip.id = "undo-closed-tab-tooltip";
        tooltip.setAttribute("hasbeenopened", "false");

        document?.getElementById("mainPopupSet")?.appendChild(tooltip);

        aNode.setAttribute("tooltip", "undo-closed-tab-tooltip");

        createRootHMR(
          () => {
            const [texts, setTexts] =
              createSignal<UndoClosedTabTexts>(defaultTexts);

            const applyTexts = (): void => {
              aNode.setAttribute("label", texts().buttonLabel);
              tooltip.setAttribute("label", texts().tooltipText);
            };

            const refreshTexts = (shortcut: string): void => {
              setTexts({
                buttonLabel: i18next.t("undo-closed-tab.label"),
                tooltipText: i18next.t("undo-closed-tab.tooltiptext", {
                  shortcut,
                }),
              });
              applyTexts();
            };

            applyTexts();

            addI18nObserver(() =>
              refreshTexts(resolveUndoCloseTabShortcut()),
            );

            // Live tooltip update when the mirrored key definitions change.
            const keySyncService = getKeySyncService();
            if (keySyncService) {
              try {
                const onKeysChanged = (keys: MirrorKeyEntry[]): void => {
                  refreshTexts(resolveShortcutFromKeys(keys));
                };
                keySyncService.addListener(onKeysChanged);
                onCleanup(() => keySyncService.removeListener(onKeysChanged));
              } catch (error) {
                console.error(
                  "[undo-closed-tab] Failed to subscribe to key mirror:",
                  error,
                );
              }
            }
          },
          import.meta.hot,
        );
      },
    );
  }
}
