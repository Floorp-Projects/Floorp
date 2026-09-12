// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act } from "react";
import { createRoot, type Root } from "react-dom/client";
import i18next, { type i18n } from "i18next";
import { I18nextProvider, initReactI18next } from "react-i18next";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuCatalogSnapshot,
} from "../../../../chrome/common/context-menu/types.ts";
import { ContextMenuEditor } from "../../../src/app/context-menu/components/ContextMenuEditor.tsx";
import { createDefaultContextMenuConfig } from "../../../src/app/context-menu/operations.ts";

const TEST_CATALOG: ContextMenuCatalogSnapshot = {
  schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
  revision: 1,
  locale: "en-US",
  surfaces: [{
    key: "content",
    label: "Web page",
    profiles: [{
      key: "default",
      label: "Default",
      containers: [{
        key: "root",
        label: "Main menu",
        complete: true,
        items: [
          {
            key: "item-a",
            label: "Alpha",
            kind: "command",
            source: "firefox",
            customizable: true,
            movable: true,
            hideable: true,
            orderAnchor: true,
            nativeHidden: false,
          },
          {
            key: "item-b",
            label: "Beta conditional",
            kind: "command",
            source: "firefox",
            customizable: true,
            movable: true,
            hideable: true,
            orderAnchor: true,
            nativeHidden: true,
          },
          {
            key: "item-c",
            label: "Gamma",
            kind: "command",
            source: "floorp",
            customizable: true,
            movable: true,
            hideable: true,
            orderAnchor: true,
            nativeHidden: false,
          },
          {
            key: "separator-a",
            label: "",
            kind: "separator",
            source: "firefox",
            customizable: false,
            movable: true,
            hideable: false,
            orderAnchor: true,
            nativeHidden: false,
          },
        ],
      }],
    }],
  }],
};

interface RenderedEditor {
  host: HTMLDivElement;
  root: Root;
  rerender(catalog: ContextMenuCatalogSnapshot, refreshing: boolean): void;
  cleanup(): void;
}

let testI18n: i18n | null = null;

async function getTestI18n(): Promise<i18n> {
  if (testI18n) return testI18n;
  testI18n = i18next.createInstance();
  await testI18n.use(initReactI18next).init({
    lng: "en",
    fallbackLng: false,
    interpolation: { escapeValue: false },
    resources: {
      en: {
        translation: {
          contextMenu: {
            separator: "Separator",
            refreshCatalog: "Refresh list",
            surface: "Browser menu",
            profile: "Context profile",
            container: "Submenu",
            menuSelection: "Menu and context",
            menuSelectionDescription: "Choose a menu",
            independentProfile: "Independent profile",
            independentProfileOnDescription: "Independent",
            independentProfileOffDescription: "Shared",
            resetProfile: "Reset profile",
            items: "Items",
            editingIndependent: "Editing independently",
            editingShared: "Editing shared layout",
            capabilityHelp: "Move items",
            viewMode: "Items shown",
            viewCurrent: "Current condition",
            viewAll: "All items",
            viewCurrentDescription: "Currently shown items",
            viewAllDescription: "All conditional items",
            allModeDragHelp: "Use Move to",
            searchItems: "Search menu items",
            moveItem: "Move {{label}}",
            moveItemUp: "Move {{label}} up",
            moveItemDown: "Move {{label}} down",
            moveUp: "Up",
            moveDown: "Down",
            moveDestinationFor: "Choose destination for {{label}}",
            moveToDestination: "Move to…",
            moveBefore: "Move before {{label}}",
            moveToEnd: "Move to end",
            movedBefore: "Moved {{label}} before {{target}}",
            movedToEnd: "Moved {{label}} to end",
            placementInstruction: "Choose where to move {{label}}",
            cancelMove: "Cancel move",
            dragItem: "Drag {{label}}",
            cannotDragItem: "Cannot drag {{label}}",
            itemVisibility: "Show {{label}}",
            itemVisibilityUnavailable: "Visibility fixed for {{label}}",
            visibilityFixed: "Fixed",
            separatorVisibilityAutomatic: "Automatic",
            nativeHidden: "Conditional",
            notMovable: "Fixed position",
            editSubmenu: "Edit submenu",
            openSubmenu: "Open {{label}}",
            kind: {
              command: "Command",
              submenu: "Submenu",
              separator: "Separator",
              group: "Group",
            },
            source: {
              firefox: "Firefox",
              floorp: "Floorp",
              extension: "Extension",
              unknown: "Unknown",
            },
          },
        },
      },
    },
  });
  return testI18n;
}

async function renderEditor(
  moveItemBefore: (
    activeKey: string,
    beforeKey: string | null | undefined,
  ) => Promise<boolean> = () => Promise.resolve(true),
): Promise<RenderedEditor> {
  const i18nInstance = await getTestI18n();
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  const render = (catalog: ContextMenuCatalogSnapshot, refreshing: boolean) => {
    root.render(
      <I18nextProvider i18n={i18nInstance}>
        <ContextMenuEditor
          catalog={catalog}
          config={createDefaultContextMenuConfig()}
          refreshing={refreshing}
          reloadCatalog={() => Promise.resolve()}
          moveItem={() => Promise.resolve(true)}
          moveItemBefore={(_target, _items, activeKey, beforeKey) =>
            moveItemBefore(activeKey, beforeKey)}
          setItemVisible={() => Promise.resolve(true)}
          setProfileIndependent={() => Promise.resolve(true)}
          resetProfile={() => Promise.resolve(true)}
        />
      </I18nextProvider>,
    );
  };
  await act(() => render(TEST_CATALOG, false));
  return {
    host,
    root,
    rerender: (catalog, refreshing) => {
      act(() => render(catalog, refreshing));
    },
    cleanup: () => {
      act(() => root.unmount());
      host.remove();
    },
  };
}

function findButton(
  host: Element,
  text: string,
  occurrence = 0,
): HTMLButtonElement {
  const matches = [...host.querySelectorAll("button")].filter((button) =>
    button.textContent?.trim() === text
  );
  const button = matches[occurrence];
  assert(button instanceof HTMLButtonElement, `button not found: ${text}`);
  return button;
}

async function nextPaint(): Promise<void> {
  await new Promise<void>((resolve) =>
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))
  );
}

async function testAllViewAvoidsSortableRowsAndNamesSearch(): Promise<void> {
  const rendered = await renderEditor();
  try {
    assertEquals(
      rendered.host.querySelectorAll("[data-context-menu-sortable-row]").length,
      3,
      "current view mounts sortable rows only for currently available items",
    );
    const search = rendered.host.querySelector('input[type="search"]');
    assert(search instanceof HTMLInputElement, "the search input is rendered");
    assertEquals(
      search.getAttribute("aria-label"),
      "Search menu items",
      "the search input has an accessible name",
    );

    await act(() => findButton(rendered.host, "All items").click());
    assertEquals(
      rendered.host.querySelectorAll("[data-context-menu-sortable-row]").length,
      0,
      "all-items view does not mount useSortable rows",
    );
    assertEquals(
      rendered.host.querySelectorAll("[data-context-menu-plain-row]").length,
      4,
      "all-items view includes the Firefox-conditional item",
    );
    assertEquals(
      findButton(rendered.host, "All items").getAttribute("aria-pressed"),
      "true",
      "view controls use toggle-button semantics",
    );

    await act(() => {
      const setter = Object.getOwnPropertyDescriptor(
        HTMLInputElement.prototype,
        "value",
      )?.set;
      assert(setter, "the native input value setter is available");
      setter.call(search, "Separator");
      search.dispatchEvent(new Event("input", { bubbles: true }));
    });
    assertEquals(
      rendered.host.querySelectorAll("[data-context-menu-plain-row]").length,
      1,
      "the localized separator label is searchable when its catalog label is empty",
    );
    assert(
      rendered.host.textContent?.includes("Separator"),
      "the localized separator fallback remains visible in search results",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testCatalogRefreshKeepsEditorState(): Promise<void> {
  const rendered = await renderEditor();
  try {
    await act(() => findButton(rendered.host, "All items").click());
    const search = rendered.host.querySelector('input[type="search"]');
    assert(search instanceof HTMLInputElement, "the search input is rendered");
    await act(() => {
      const setter = Object.getOwnPropertyDescriptor(
        HTMLInputElement.prototype,
        "value",
      )?.set;
      assert(setter, "the native input value setter is available");
      setter.call(search, "beta");
      search.dispatchEvent(new Event("input", { bubbles: true }));
    });

    rendered.rerender({ ...TEST_CATALOG, revision: 2 }, true);
    assertEquals(search.value, "beta", "refresh preserves the search query");
    assertEquals(
      findButton(rendered.host, "All items").getAttribute("aria-pressed"),
      "true",
      "refresh preserves the selected view",
    );
    const refresh = findButton(rendered.host, "Refresh list");
    assert(refresh.disabled, "refresh is disabled while a request is pending");
    assert(
      refresh.querySelector(".animate-spin") !== null,
      "refresh communicates pending state without unmounting the editor",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testIncompleteCatalogUsesAllItemsView(): Promise<void> {
  const rendered = await renderEditor();
  try {
    const rootContainer = TEST_CATALOG.surfaces[0].profiles[0].containers[0];
    rendered.rerender({
      ...TEST_CATALOG,
      revision: 2,
      surfaces: [{
        ...TEST_CATALOG.surfaces[0],
        profiles: [{
          ...TEST_CATALOG.surfaces[0].profiles[0],
          containers: [{ ...rootContainer, complete: false }],
        }],
      }],
    }, false);

    assertEquals(
      findButton(rendered.host, "All items").getAttribute("aria-pressed"),
      "true",
      "a provisional cold-start catalog exposes all structurally known rows",
    );
    assert(
      findButton(rendered.host, "Current condition").disabled,
      "current-condition filtering stays unavailable until Firefox observes a real context",
    );
    assertEquals(
      rendered.host.querySelectorAll("[data-context-menu-plain-row]").length,
      4,
      "nativeHidden from the provisional DOM does not hide catalog rows",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testInitialSelectionPrefersWebPage(): Promise<void> {
  const rendered = await renderEditor();
  try {
    const originalSurface = TEST_CATALOG.surfaces[0];
    const genericSurface = {
      ...originalSurface,
      key: "browser.chrome.customizationPanelItemContextMenu",
      label: "Customization Panel Item",
    };
    const contentSurface = {
      ...originalSurface,
      key: "browser.content",
      profiles: [{
        ...originalSurface.profiles[0],
        key: "page",
        label: "Page",
      }],
    };
    rendered.rerender({
      ...TEST_CATALOG,
      revision: 2,
      surfaces: [genericSurface, contentSurface],
    }, false);

    const selects = rendered.host.querySelectorAll("select");
    assertEquals(
      selects[0]?.value,
      "browser.content",
      "generic discovered menus do not displace Web page as the useful initial target",
    );
    assertEquals(
      findButton(rendered.host, "Page").getAttribute("aria-pressed"),
      "true",
      "the Web page context opens on its Page profile",
    );
  } finally {
    rendered.cleanup();
  }
}

async function testPlacementIncludesConditionalGapAndRestoresFocus(): Promise<
  void
> {
  let activeKey = "";
  let beforeKey: string | null | undefined;
  const rendered = await renderEditor((nextActiveKey, nextBeforeKey) => {
    activeKey = nextActiveKey;
    beforeKey = nextBeforeKey;
    return Promise.resolve(true);
  });
  try {
    const moveButtons = [...rendered.host.querySelectorAll("button")].filter(
      (button) => button.textContent?.trim() === "Move to…",
    );
    const gammaMove = moveButtons[1];
    assert(
      gammaMove instanceof HTMLButtonElement,
      "the current view exposes a move button for Gamma",
    );
    await act(() => gammaMove.click());
    await act(nextPaint);

    const conditionalGap = [...rendered.host.querySelectorAll(
      '[data-context-menu-placement-gap="true"]',
    )].find((gap) => gap.textContent?.includes("Beta conditional"));
    assert(
      conditionalGap instanceof HTMLButtonElement,
      "placement exposes the gap before a Firefox-conditional item",
    );
    await act(async () => {
      conditionalGap.click();
      await Promise.resolve();
    });
    await act(nextPaint);
    assertEquals(activeKey, "item-c", "placement reports the moving item");
    assertEquals(
      beforeKey,
      "item-b",
      "placement reports the exact conditional destination",
    );
    assertEquals(
      rendered.host.querySelectorAll('[data-context-menu-placement-gap="true"]')
        .length,
      0,
      "successful placement closes destination mode",
    );
    const restoredGammaMove = [...rendered.host.querySelectorAll("button")]
      .filter((button) => button.textContent?.trim() === "Move to…")[1];
    assertEquals(
      document.activeElement,
      restoredGammaMove,
      "successful placement restores focus to the originating button",
    );

    assert(
      restoredGammaMove instanceof HTMLButtonElement,
      "the Gamma move button is rendered again after placement",
    );
    await act(() => restoredGammaMove.click());
    await act(nextPaint);
    await act(async () => {
      findButton(rendered.host, "Move to end").click();
      await Promise.resolve();
    });
    await act(nextPaint);
    assertEquals(activeKey, "item-c", "end placement reports the moving item");
    assertEquals(
      beforeKey,
      null,
      "the end gap moves the item after the trailing separator",
    );

    const afterEndGammaMove = [...rendered.host.querySelectorAll("button")]
      .filter((button) => button.textContent?.trim() === "Move to…")[1];
    assert(
      afterEndGammaMove instanceof HTMLButtonElement,
      "the Gamma move button is restored after end placement",
    );
    await act(() => afterEndGammaMove.click());
    await act(nextPaint);
    await act(async () => {
      globalThis.dispatchEvent(
        new KeyboardEvent("keydown", { key: "Escape" }),
      );
      await nextPaint();
    });
    assertEquals(
      rendered.host.querySelectorAll('[data-context-menu-placement-gap="true"]')
        .length,
      0,
      "Escape cancels destination mode",
    );
    const cancelledGammaMove = [...rendered.host.querySelectorAll("button")]
      .filter((button) => button.textContent?.trim() === "Move to…")[1];
    assertEquals(
      document.activeElement,
      cancelledGammaMove,
      "Escape restores focus to the originating button",
    );
  } finally {
    rendered.cleanup();
  }
}

const tests: TestCase[] = [
  {
    name: "all view avoids sortable rows and names search",
    fn: testAllViewAvoidsSortableRowsAndNamesSearch,
  },
  {
    name: "catalog refresh keeps editor state",
    fn: testCatalogRefreshKeepsEditorState,
  },
  {
    name: "incomplete catalog uses the all-items view",
    fn: testIncompleteCatalogUsesAllItemsView,
  },
  {
    name: "initial selection prefers the Web page menu",
    fn: testInitialSelectionPrefersWebPage,
  },
  {
    name: "placement includes conditional gap and restores focus",
    fn: testPlacementIncludesConditionalGapAndRestoresFocus,
  },
];

export async function runAllTests(): Promise<void> {
  const environment = globalThis as typeof globalThis & {
    IS_REACT_ACT_ENVIRONMENT?: boolean;
  };
  environment.IS_REACT_ACT_ENVIRONMENT = true;
  await runTests("ContextMenuEditor.test.tsx", tests);
}
