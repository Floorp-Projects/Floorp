// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { panelSidebarData, setPanelSidebarData } from "../data/data.ts";
import {
  getConfig,
  isEnabled,
  setConfig,
  setEnabled,
} from "../../keyboard-shortcut/config.ts";
import { CHROME_SIDE_PANEL_ACTION_ID } from "../../keyboard-shortcut/actions.ts";
import { getKeyboardShortcutActionOptions } from "../../../../pages-settings/src/app/keyboard-shortcut/actionCatalog.ts";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

async function until(condition: () => boolean, label = "state"): Promise<void> {
  const end = Date.now() + 10000;
  while (Date.now() < end) {
    if (condition()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  throw new Error("Side panel UI did not reach " + label);
}

export async function runAllTests(): Promise<string[]> {
  const checks: string[] = [];
  const extensionId = "a".repeat(32) + "@chromium-extension";
  const previous = panelSidebarData();
  const enabled = isEnabled();
  const config = getConfig();
  try {
    const panel = {
      id: "floorp-chrome-side-panel-test",
      type: "extension" as const,
      width: 360,
      url: null,
      icon: null,
      userContextId: null,
      zoomLevel: null,
      userAgent: null,
      extensionId,
    };
    setPanelSidebarData([...previous, panel]);
    // The packaged feature reacts to the real stored panel configuration.
    // Use that production button rather than rendering a duplicate ID whose
    // selection marker competes with the production sidebar manager.
    const selector = `.panel-sidebar-panel[data-panel-id="${panel.id}"]`;
    await until(
      () => document.querySelectorAll(selector).length === 1,
      "one production pin",
    );
    const button = document.querySelector(selector) as HTMLElement;
    assert(button, "The existing pinned-extension button renders");
    checks.push(
      "Existing Floorp extension pin renders the Chrome side-panel entry",
    );
    if (window.SidebarController.isOpen) window.SidebarController.hide();
    button.click();
    await until(() => window.SidebarController.isOpen, "open");
    assert(
      !document.getElementById(`sidebar-panel-${panel.id}`),
      "Native Chrome panels do not create a duplicate Floorp content browser",
    );
    await until(
      () => button.getAttribute("data-checked") === "true",
      "active pin marker",
    );
    checks.push(
      "Clicking a Floorp pin opens the native Chrome scope and marks the existing button active",
    );
    button.click();
    await until(() => !window.SidebarController.isOpen, "closed");
    checks.push("Clicking the active Floorp pin closes its native panel");
    const actions = getKeyboardShortcutActionOptions((_key, fallback) =>
      fallback
    );
    assert(
      actions.some((action) => action.id === CHROME_SIDE_PANEL_ACTION_ID),
      "The existing shortcut settings catalog includes the side-panel action",
    );
    checks.push(
      "The existing keyboard settings catalog exposes a configurable side-panel action",
    );
    setEnabled(true);
    setConfig({
      schemaVersion: 2,
      enabled: true,
      shortcuts: {
        test: {
          key: "KeyY",
          modifiers: { alt: false, ctrl: true, meta: false, shift: true },
          action: CHROME_SIDE_PANEL_ACTION_ID,
        },
      },
    });
    await until(
      () =>
        Reflect.get(window, "__keyboardShortcutControllerAttached") === true,
      "production keyboard controller",
    );
    const chromeWindow = window;
    const shortcut = () => {
      chromeWindow.dispatchEvent(
        new KeyboardEvent("keydown", {
          key: "Y",
          code: "KeyY",
          ctrlKey: true,
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        }),
      );
      chromeWindow.dispatchEvent(
        new KeyboardEvent("keyup", {
          key: "Y",
          code: "KeyY",
          ctrlKey: true,
          shiftKey: true,
          bubbles: true,
        }),
      );
    };
    shortcut();
    await until(() => window.SidebarController.isOpen, "open");
    shortcut();
    await until(() => !window.SidebarController.isOpen, "closed");
    checks.push(
      "Configured keyboard events toggle the last pinned Chrome side panel through the real controller",
    );
    return checks;
  } finally {
    setEnabled(enabled);
    setConfig(config);
    setPanelSidebarData(previous);
  }
}
