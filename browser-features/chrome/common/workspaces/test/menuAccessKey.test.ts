// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getWorkspaceMenuAccessKey } from "../utils/menu-accesskey.ts";
import { WorkspacesTabContextMenu } from "../tabContextMenu.tsx";
import type { WorkspacesService } from "../workspacesService.ts";
import type { TabContextMenuPopup } from "./menu-accesskey-test-types.ts";
import { createRoot } from "solid-js";
import i18next from "i18next";
import { setLanguage } from "#i18n/config-browser-chrome.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function withMenu(test: (menu: Element, popup: Element) => void): void {
  // Closed XUL popups have no computed styles; use a laid-out, offscreen parent.
  const popup = document.createXULElement("vbox");
  popup.setAttribute("style", "position: fixed; left: -10000px; top: 0");
  const menu = document.createXULElement("menu");
  menu.setAttribute("accesskey", "K");
  popup.appendChild(menu);
  document.documentElement.appendChild(popup);
  try {
    test(menu, popup);
  } finally {
    popup.remove();
  }
}

function addCommand(popup: Element, key: string): Element {
  const command = document.createXULElement("menuitem");
  command.setAttribute("accesskey", key);
  popup.appendChild(command);
  return command;
}

async function withTabMenu(
  test: (menu: Element, popup: TabContextMenuPopup) => Promise<void>,
): Promise<void> {
  const ids = [
    "tabContextMenu",
    "context_moveTabOptions",
    "context_MoveTabToOtherWorkspace",
    "WorkspacesTabContextMenu",
  ];
  const existingElements = ids.map((id) => ({
    id,
    element: document.getElementById(id),
  }));
  for (const { element } of existingElements) {
    element?.removeAttribute("id");
  }
  const popup = document.createXULElement("menupopup") as TabContextMenuPopup;
  popup.id = "tabContextMenu";
  const marker = document.createXULElement("menu");
  marker.id = "context_moveTabOptions";
  marker.setAttribute("label", "Other tab commands");
  popup.appendChild(marker);
  document.documentElement.appendChild(popup);
  let dispose = () => {};
  try {
    createRoot((cleanup) => {
      dispose = cleanup;
      // Parent-menu tests do not open the workspace submenu or use the service.
      new WorkspacesTabContextMenu(Object.create(null) as WorkspacesService);
    });
    const menu = popup.querySelector("#context_MoveTabToOtherWorkspace");
    assert(menu, "the isolated workspace tab menu must be initialized");
    await test(menu, popup);
  } finally {
    popup.hidePopup();
    dispose();
    popup.remove();
    for (const { id, element } of existingElements) {
      element?.setAttribute("id", id);
    }
  }
}

function testLocalizedKeyIsPreferred(): void {
  withMenu((menu, popup) => {
    addCommand(popup, "D");
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "J"),
      "J",
      "an available localized key must take precedence over source fallbacks",
    );
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "K"),
      "K",
      "the workspace menu must not conflict with itself",
    );
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "d"),
      "K",
      "a localized key must not collide with Duplicate Tab",
    );
  });
}

function testChineseMultiselectCollision(): void {
  withMenu((menu, popup) => {
    // zh-CN classic menu: new tab W, duplicate D, bookmark selected tabs K.
    addCommand(popup, "w");
    addCommand(popup, "D");
    const bookmark = addCommand(popup, "k");
    bookmark.setAttribute("hidden", "true");
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "K"),
      "K",
      "a hidden multiselect command must not consume the key",
    );
    bookmark.removeAttribute("hidden");
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "K"),
      "F",
      "source fallback K must not collide when the multiselect command appears",
    );
  });
}

function testHiddenCommandsAndSubmenuKeys(): void {
  withMenu((menu, popup) => {
    const hidden = addCommand(popup, "K");
    hidden.setAttribute("hidden", "true");
    const collapsed = addCommand(popup, "K");
    collapsed.setAttribute("collapsed", "true");
    const cssHidden = addCommand(popup, "K");
    cssHidden.setAttribute("style", "display: none !important");
    const submenu = document.createXULElement("menu");
    const nestedPopup = document.createXULElement("menupopup");
    addCommand(nestedPopup, "K");
    submenu.appendChild(nestedPopup);
    popup.appendChild(submenu);
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "K"),
      "K",
      "hidden commands and keys in other submenus must not consume the key",
    );
  });
}

function testInvalidTranslationDoesNotBecomeAnAccessKey(): void {
  withMenu((menu, popup) => {
    addCommand(popup, "k");
    assertEquals(
      getWorkspaceMenuAccessKey(
        menu,
        "workspaces.menu.moveTabToAnotherWorkspaceAccessKey",
      ),
      "W",
      "an untranslated message id must use a valid, available fallback",
    );
  });
}

function testExhaustedKeys(): void {
  withMenu((menu, popup) => {
    for (const key of ["K", "w", "f"]) {
      addCommand(popup, key);
    }
    assertEquals(
      getWorkspaceMenuAccessKey(menu, "K"),
      "",
      "no access key is safer than a duplicate when all fallbacks are taken",
    );
  });
}

async function testLanguageChangeUpdatesLabelAndKey(): Promise<void> {
  await withTabMenu(async (menu) => {
    const key = "workspaces.menu.moveTabToAnotherWorkspaceAccessKey";
    const originalLocale = i18next.language;
    const originalKey: unknown = i18next.getResource(
      "ja-JP",
      "browser-chrome",
      key,
    );
    try {
      if (originalLocale === "ja-JP") {
        await i18next.changeLanguage("en-US");
        setLanguage("en-US");
      }
      i18next.addResource("ja-JP", "browser-chrome", key, "J");
      await i18next.changeLanguage("ja-JP");
      setLanguage("ja-JP");
      await Promise.resolve();
      assertEquals(
        menu.getAttribute("label"),
        i18next.t("workspaces.menu.moveTabToAnotherWorkspace"),
        "locale changes must update the label",
      );
      assertEquals(
        menu.getAttribute("accesskey"),
        "J",
        "locale changes must also update the access key",
      );
    } finally {
      if (typeof originalKey === "string") {
        i18next.addResource("ja-JP", "browser-chrome", key, originalKey);
      } else {
        const menuResources: object = i18next.getResource(
          "ja-JP",
          "browser-chrome",
          "workspaces.menu",
        );
        Reflect.deleteProperty(
          menuResources,
          "moveTabToAnotherWorkspaceAccessKey",
        );
      }
      await i18next.changeLanguage(originalLocale);
      setLanguage(originalLocale);
    }
  });
}

async function testPendingTranslationUsesSourceFallback(): Promise<void> {
  await withTabMenu(async (menu) => {
    const key = "workspaces.menu.moveTabToAnotherWorkspaceAccessKey";
    const originalLocale = i18next.language;
    const originalKey: unknown = i18next.getResource(
      "zh-CN",
      "browser-chrome",
      key,
    );
    const menuResources: object = i18next.getResource(
      "zh-CN",
      "browser-chrome",
      "workspaces.menu",
    );
    try {
      Reflect.deleteProperty(
        menuResources,
        "moveTabToAnotherWorkspaceAccessKey",
      );
      if (originalLocale === "zh-CN") {
        await i18next.changeLanguage("en-US");
        setLanguage("en-US");
      }
      await i18next.changeLanguage("zh-CN");
      setLanguage("zh-CN");
      await Promise.resolve();
      assertEquals(
        i18next.t(key),
        "K",
        "pending Crowdin translations must use the English source key",
      );
      const accessKey = menu.getAttribute("accesskey");
      assert(
        accessKey,
        "pending translations must keep an available access key",
      );
      assertEquals(
        Array.from(accessKey).length,
        1,
        "a missing locale key must not expose the translation identifier",
      );
    } finally {
      if (typeof originalKey === "string") {
        i18next.addResource("zh-CN", "browser-chrome", key, originalKey);
      }
      await i18next.changeLanguage(originalLocale);
      setLanguage(originalLocale);
    }
  });
}

async function testLateNativeKeysAreRechecked(): Promise<void> {
  await withTabMenu(async (menu, popup) => {
    const originalKey = menu.getAttribute("accesskey");
    assert(originalKey, "the workspace menu must have a key");
    const command = document.createXULElement("menuitem");
    try {
      popup.appendChild(command);
      command.setAttribute("accesskey", originalKey.toLowerCase());
      await Promise.resolve();
      const updatedKey = menu.getAttribute("accesskey");
      assert(updatedKey, "an available fallback must be retained");
      assert(
        updatedKey.toLowerCase() !== originalKey.toLowerCase(),
        "a late native Fluent key must not collide with the workspace key",
      );
    } finally {
      command.remove();
      await Promise.resolve();
    }
    assertEquals(
      menu.getAttribute("accesskey"),
      originalKey,
      "removing the conflicting command must restore the preferred key",
    );
  });
}

async function testCssVisibilityChangesAreRechecked(): Promise<void> {
  await withTabMenu(async (menu, popup) => {
    const originalKey = menu.getAttribute("accesskey");
    assert(originalKey, "the workspace menu must have a key");
    const command = addCommand(popup, originalKey.toLowerCase());
    command.setAttribute("label", "Native command");
    command.setAttribute("style", "display: none !important");
    await new Promise<void>((resolve) => {
      popup.addEventListener("popupshown", () => resolve(), { once: true });
      popup.openPopup(gBrowser.selectedTab, "after_start", 0, 0, true, false);
    });
    assertEquals(
      menu.getAttribute("accesskey"),
      originalKey,
      "a command hidden with CSS must not consume the key",
    );
    command.removeAttribute("style");
    await Promise.resolve();
    const updatedKey = menu.getAttribute("accesskey");
    assert(updatedKey, "an available fallback must be retained");
    assert(
      updatedKey.toLowerCase() !== originalKey.toLowerCase(),
      "a native command revealed through CSS must trigger collision avoidance",
    );
    command.setAttribute("style", "display: none !important");
    await Promise.resolve();
    assertEquals(
      menu.getAttribute("accesskey"),
      originalKey,
      "hiding the conflicting command must restore the preferred key",
    );
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "localized key is preferred", fn: testLocalizedKeyIsPreferred },
    {
      name: "zh-CN multiselect key collision",
      fn: testChineseMultiselectCollision,
    },
    { name: "hidden and nested keys", fn: testHiddenCommandsAndSubmenuKeys },
    {
      name: "untranslated access key uses a valid fallback",
      fn: testInvalidTranslationDoesNotBecomeAnAccessKey,
    },
    { name: "exhausted keys do not create a collision", fn: testExhaustedKeys },
    {
      name: "language change updates label and access key",
      fn: testLanguageChangeUpdatesLabelAndKey,
    },
    {
      name: "late native keys are rechecked",
      fn: testLateNativeKeysAreRechecked,
    },
    {
      name: "pending Crowdin translations use source fallback",
      fn: testPendingTranslationUsesSourceFallback,
    },
    {
      name: "CSS visibility changes recheck the key",
      fn: testCssVisibilityChangesAreRechecked,
    },
  ];
  await runTests("menuAccessKey.test.ts", tests);
}
