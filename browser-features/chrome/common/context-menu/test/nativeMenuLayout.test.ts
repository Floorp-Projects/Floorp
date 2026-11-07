// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  NativeMenuL10nElement,
  NativeMenuLayoutModule,
  NativeMenuMode,
  NativeTabContextMenu,
} from "./native-menu-layout-types.ts";

const FLOORP_IDS = [
  "context_openInExternalBrowser",
  "context_MoveTabToOtherWorkspace",
  "context_toggleToPrivateContainer",
];

const native = globalThis.TabContextMenu as unknown as NativeTabContextMenu;
const { MenuSectionLayout } = ChromeUtils.importESModule(
  "resource:///modules/MenuSectionLayout.sys.mjs",
) as NativeMenuLayoutModule;

function item(popup: Element, id: string): Element {
  const matches = popup.querySelectorAll(`#${id}`);
  assertEquals(matches.length, 1, `Exactly one native menu node #${id}`);
  return matches[0];
}

function cloneNativeMenu(): Element {
  const original = document.getElementById("tabContextMenu");
  assert(original, "The actual native tab context menu is available");
  for (const id of FLOORP_IDS) item(original, id);
  const popup = original.cloneNode(true) as Element;
  // Stay detached: duplicate native IDs and command attributes must never
  // participate in the live menu. Preserve the native Fluent cache, which
  // cloneNode does not copy when the live menu has already changed layout.
  const originalLabels = original.querySelectorAll<NativeMenuL10nElement>(
    "[data-alt-l10n-id]",
  );
  const labels = popup.querySelectorAll<NativeMenuL10nElement>(
    "[data-alt-l10n-id]",
  );
  for (let i = 0; i < labels.length; i++) {
    if (originalLabels[i]._classicL10nId != null) {
      labels[i]._classicL10nId = originalLabels[i]._classicL10nId;
    }
  }
  assert(!popup.isConnected, "The native clone is isolated from the live menu");
  return popup;
}

function arrange(popup: Element, mode: NativeMenuMode): void {
  new MenuSectionLayout(native.MENU_SECTIONS[mode], {
    dynamicItemSelectors: native.DYNAMIC_MENU_ITEM_SELECTORS,
  }).arrange(popup);
  // This native helper is scoped to its popup. Do not call _ensureMenuArranged:
  // its unused-item helper queries the live document instead of the clone.
  native._updateL10nIds(popup, mode === "altstructure");
}

function assertNativePositions(popup: Element, mode: NativeMenuMode): void {
  const alternate = mode === "altstructure";
  assertEquals(
    item(popup, "context_selectAllTabs").parentElement?.id,
    alternate ? "moveTabOptionsMenu" : "tabContextMenu",
    "Select All Tabs follows the native layout's submenu relocation",
  );
  assertEquals(
    item(popup, "context_closeDuplicateTabs").parentElement?.id,
    alternate ? "closeTabOptions" : "tabContextMenu",
    "Close Duplicate Tabs follows the native layout's submenu relocation",
  );
  assertEquals(
    item(popup, "context_moveTabToGroupNewGroup").parentElement?.id,
    alternate ? "moveTabOptionsMenu" : "context_moveTabToGroupPopupMenu",
    "New Group returns to its native parent when switching back to classic",
  );
  assertEquals(
    item(popup, "context_reloadTab").getAttribute("data-l10n-id"),
    alternate ? "reload-tab2" : "reload-tab",
    "The actual native helper applies and restores static Fluent labels",
  );
}

function testNativeLayoutCyclePreservesFloorpEntries(): void {
  const popup = cloneNativeMenu();
  const nodes = FLOORP_IDS.map((id) => item(popup, id));
  const labels = nodes.map((node) => node.getAttribute("label"));
  const calls = [0, 0, 0];
  const event = "floorp-menu-layout-regression";
  // cloneNode does not copy production listeners. These inert listeners verify
  // that native moves preserve node/listener identity without running commands.
  nodes.forEach((node, i) => node.addEventListener(event, () => calls[i]++));

  for (const mode of ["classic", "altstructure", "classic"] as const) {
    arrange(popup, mode);
    assertNativePositions(popup, mode);
    for (const [i, id] of FLOORP_IDS.entries()) {
      assertEquals(
        item(popup, id),
        nodes[i],
        "Layout preserves the Floorp node",
      );
      assertEquals(
        nodes[i].parentElement,
        popup,
        "Floorp entry remains top-level",
      );
      assertEquals(
        nodes[i].getAttribute("label"),
        labels[i],
        "Floorp label survives",
      );
      nodes[i].dispatchEvent(new Event(event));
    }
    assertEquals(
      nodes[0].nextElementSibling,
      nodes[1],
      "External precedes Workspace",
    );
    assertEquals(
      nodes[1].nextElementSibling,
      item(popup, "context_moveTabOptions"),
      "Workspace precedes the native Move Tabs entry",
    );
    assertEquals(
      nodes[2].previousElementSibling,
      item(popup, "context_reopenInContainer"),
      "Private Container follows the native container entry in either mode",
    );
    const after = popup.outerHTML;
    arrange(popup, mode);
    assertEquals(
      popup.outerHTML,
      after,
      "Reapplying the native layout is idempotent",
    );
  }
  for (const count of calls) {
    assertEquals(
      count,
      3,
      "The same listener survives all three native arrangements",
    );
  }
}

function testNativeLayoutAllowsMissingOptionalFloorpEntries(): void {
  const popup = cloneNativeMenu();
  for (const id of FLOORP_IDS) item(popup, id).remove();
  for (const mode of ["classic", "altstructure", "classic"] as const) {
    arrange(popup, mode);
    assertNativePositions(popup, mode);
    for (const id of FLOORP_IDS) {
      assertEquals(
        popup.querySelector(`#${id}`),
        null,
        "Missing optional entry stays absent",
      );
    }
  }
}

function testNativeLayoutStillRejectsUnknownMiddleItemAtomically(): void {
  for (const mode of ["classic", "altstructure"] as const) {
    const popup = cloneNativeMenu();
    const unknown = document.createXULElement("menuitem");
    unknown.id = "floorp-native-layout-unclaimed-test";
    popup.insertBefore(unknown, item(popup, "context_moveTabOptions"));
    const before = popup.outerHTML;
    let message = "";
    try {
      new MenuSectionLayout(native.MENU_SECTIONS[mode], {
        dynamicItemSelectors: native.DYNAMIC_MENU_ITEM_SELECTORS,
      }).arrange(popup);
    } catch (error) {
      message = String(error);
    }
    assert(
      message.includes("not placed by any section") &&
        message.includes("#floorp-native-layout-unclaimed-test"),
      "The native error identifies the unregistered item",
    );
    assertEquals(
      popup.outerHTML,
      before,
      "Coverage failure does not partially reorder the menu",
    );
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("nativeMenuLayout.test.ts", [
    {
      name:
        "native layouts preserve Floorp entries through classic/alternate/classic",
      fn: testNativeLayoutCyclePreservesFloorpEntries,
    },
    {
      name: "native layouts allow Floorp entries before feature injection",
      fn: testNativeLayoutAllowsMissingOptionalFloorpEntries,
    },
    {
      name:
        "native coverage still rejects unknown middle entries before commit",
      fn: testNativeLayoutStillRejectsUnknownMiddleItemAtomically,
    },
  ]);
}
