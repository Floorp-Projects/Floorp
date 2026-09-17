// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  button,
  click,
  element,
  executeCases,
  faults,
  input,
  pause,
  prefs,
  runFixture,
  until,
  valueAt,
  writes,
} from "../../../../libs/ui/test/page-fixture.ts";

const DESIGN = "floorp.design.configs";
const WORKSPACES = "floorp.workspaces.v4.config";
const PANEL = "floorp.panelSidebar.config";
const PWA = "floorp.browser.ssb.config";
const MEMORY = "floorp.memory.idleReclaim";
async function dismissDialog() {
  document.dispatchEvent(
    new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
  );
  await pause();
}
export async function route(path: string, selector: string) {
  await dismissDialog();
  await click('a[href="/overview/home"]');
  await click(`a[href="/${path}"]`);
  await until(
    () => document.querySelector(selector),
    `Route ${path} did not render`,
  );
  await until(
    () => !document.querySelector("fieldset[disabled]"),
    `Route ${path} is still loading`,
  );
  await pause();
}
export async function toggle(selector: string, key: string, path?: string) {
  const target = element<HTMLInputElement>(selector);
  const expected = !target.checked;
  const before = writes.length;
  await click(selector);
  await until(
    () =>
      writes.length > before &&
      (path ? valueAt(key, path) : prefs.get(key)) === expected,
    `${selector} did not save ${expected}`,
  );
  assertEquals(
    element<HTMLInputElement>(selector).checked,
    expected,
    `${selector} rendered state`,
  );
}
const designToggles = [
  ["favicon-color", "globalConfigs.faviconColor"],
  ["reverse-scroll", "tab.tabScroll.reverse"],
  ["scroll-wrap", "tab.tabScroll.wrap"],
  ["scroll-tab", "tab.tabScroll.enabled"],
  ["pin-title", "tab.tabPinTitle"],
  ["double-click-close", "tab.tabDoubleClickToClose"],
  ["search-bar-top", "uiCustomization.navbar.searchBarTop"],
  [
    "disable-fullscreen-notification",
    "uiCustomization.display.disableFullscreenNotification",
  ],
  ["delete-browser-border", "uiCustomization.display.deleteBrowserBorder"],
  ["disable-qr-code-button", "uiCustomization.qrCode.disableButton"],
  ["disable-floorp-start", "uiCustomization.disableFloorpStart"],
  [
    "optimize-for-tree-style-tab",
    "uiCustomization.special.optimizeForTreeStyleTab",
  ],
  [
    "hide-forward-backward-button",
    "uiCustomization.special.hideForwardBackwardButton",
  ],
  ["stg-like-workspaces", "uiCustomization.special.stgLikeWorkspaces"],
  ["bookmark-bar-focus-expand", "uiCustomization.bookmarkBar.focusExpand"],
] as const;

export async function runPageTests() {
  await until(
    () => document.querySelector('a[href="/features/design"]'),
    "App did not start",
    15000,
  );
  const tests: TestCase[] = [];
  tests.push({
    name:
      "Legacy double-click preference loads and saves under the corrected key",
    fn: async () => {
      const original = prefs.get(DESIGN);
      try {
        for (const enabled of [true, false]) {
          const saved = JSON.parse(String(original));
          delete saved.tab.tabDoubleClickToClose;
          saved.tab.tabDubleClickToClose = enabled;
          prefs.set(DESIGN, JSON.stringify(saved));
          await route("features/design", "#double-click-close");
          assertEquals(
            element<HTMLInputElement>("#double-click-close").checked,
            enabled,
            "Legacy value should be displayed",
          );
          await toggle(
            "#double-click-close",
            DESIGN,
            "tab.tabDoubleClickToClose",
          );
          const updated = JSON.parse(String(prefs.get(DESIGN)));
          assert(
            !Object.hasOwn(updated.tab, "tabDubleClickToClose"),
            "Saving must remove the old key",
          );
          assertEquals(
            updated.futureKey,
            "keep",
            "Unrelated settings survive migration",
          );
          await route("features/design", "#double-click-close");
          assertEquals(
            element<HTMLInputElement>("#double-click-close").checked,
            !enabled,
            "Saved value survives reload",
          );
        }
      } finally {
        prefs.set(DESIGN, original);
      }
    },
  });
  tests.push({
    name: "Design initial load does not write settings",
    fn: async () => {
      writes.length = 0;
      await route("features/design", "#favicon-color");
      assert(
        !writes.some((write) => write.method === DESIGN),
        "Opening design rewrote preferences",
      );
    },
  });
  for (const [id, path] of designToggles) {
    tests.push({
      name: `Design ${id} saves and reloads`,
      fn: async () => {
        await route("features/design", `#${id}`);
        await toggle(`#${id}`, DESIGN, path);
        const expected = valueAt(DESIGN, path);
        await route("features/design", `#${id}`);
        assertEquals(
          element<HTMLInputElement>(`#${id}`).checked,
          expected,
          "Saved value reloads",
        );
      },
    });
  }
  for (
    const [selector, path, values] of [
      ['input[name="design"]', "globalConfigs.userInterface", [
        "proton",
        "photon",
        "protonfix",
        "fluerial",
        "lepton",
      ]],
      ['input[name="style"]', "tabbar.tabbarStyle", [
        "vertical",
        "multirow",
        "horizontal",
      ]],
      ['input[name="position"]', "tabbar.tabbarPosition", [
        "hide-horizontal-tabbar",
        "optimise-to-vertical-tabbar",
        "bottom-of-navigation-toolbar",
        "bottom-of-window",
        "default",
      ]],
      ['input[name="tabOpenPosition"]', "tab.tabOpenPosition", [0, 1, -1]],
      ['input[name="navbarPosition"]', "uiCustomization.navbar.position", [
        "bottom",
        "top",
      ]],
      [
        'input[name="bookmarkBarPosition"]',
        "uiCustomization.bookmarkBar.position",
        ["bottom", "top"],
      ],
    ] as const
  ) {
    tests.push({
      name: `Design ${path} choices`,
      fn: async () => {
        await route("features/design", "#favicon-color");
        for (const value of values) {
          await click(`${selector}[value="${value}"]`);
          await until(
            () => valueAt(DESIGN, path) === value,
            `${path} did not save ${value}`,
          );
        }
      },
    });
  }
  tests.push({
    name: "Design numeric fields and multirow settings",
    fn: async () => {
      await route("features/design", "#tab-min-width");
      for (
        const [selector, path, value] of [[
          "#tab-min-width",
          "tab.tabMinWidth",
          120,
        ], ["#tab-min-height", "tab.tabMinHeight", 40]] as const
      ) {
        await input(selector, String(value));
        await until(
          () => valueAt(DESIGN, path) === value,
          `${path} numeric save`,
        );
      }
      await click('input[name="style"][value="multirow"]');
      await toggle(
        'input[name="maxRowEnabled"]',
        DESIGN,
        "tabbar.multiRowTabBar.maxRowEnabled",
      );
      await input('input[name="maxRow"]', "5");
      await until(
        () => valueAt(DESIGN, "tabbar.multiRowTabBar.maxRow") === 5,
        "Row count saves",
      );
      await toggle(
        "#multirow-tab-newtab-inside",
        DESIGN,
        "uiCustomization.multirowTab.newtabInsideEnabled",
      );
      await route("features/design", 'input[name="maxRow"]');
      assertEquals(
        element<HTMLInputElement>('input[name="maxRow"]').value,
        "5",
        "Row count reloads",
      );
    },
  });
  tests.push({
    name: "Design preserves unexposed nested preferences",
    fn: () => {
      assertEquals(
        valueAt(DESIGN, "futureKey"),
        "keep",
        "Top-level key survives",
      );
      assertEquals(
        valueAt(DESIGN, "uiCustomization.navbar.futureKey"),
        "keep",
        "Nested key survives",
      );
    },
  });
  for (
    const [id, key] of [
      ["split-view-dnd-create", "floorp.splitView.dragToSplitCreate.enabled"],
      ["enable-tab-stacks", "floorp.tabstacks.enabled"],
      ["taskbar-tab-previews", "browser.taskbar.previews.enable"],
    ]
  ) {
    tests.push({
      name: `${id} independent preference`,
      fn: async () => {
        await route("features/design", `#${id}`);
        await toggle(`#${id}`, key);
        const expected = prefs.get(key);
        await route("features/design", `#${id}`);
        assertEquals(
          element<HTMLInputElement>(`#${id}`).checked,
          expected,
          "Independent preference reloads",
        );
      },
    });
  }
  tests.push({
    name: "New window behavior choices",
    fn: async () => {
      await route("features/design", "#open-new-window-behavior");
      for (const value of [1, 2, 3]) {
        await input("#open-new-window-behavior", String(value));
        await until(
          () => prefs.get("browser.link.open_newwindow") === value,
          "Window behavior save",
        );
      }
    },
  });
  for (
    const [routeName, id, key, path] of [
      ["workspaces", "enable-workspaces", "floorp.workspaces.enabled", ""],
      ["workspaces", "close-popup", WORKSPACES, "closePopupAfterClick"],
      ["workspaces", "show-name", WORKSPACES, "showWorkspaceNameOnToolbar"],
      [
        "workspaces",
        "exit-on-last-tab-close",
        WORKSPACES,
        "exitOnLastTabClose",
      ],
      ["workspaces", "manage-bms", WORKSPACES, "manageOnBms"],
      ["sidebar", "enable-panel", "floorp.panelSidebar.enabled", ""],
      ["sidebar", "auto-unload", PANEL, "autoUnload"],
      ["webapps", "enable-pwa", "floorp.browser.ssb.enabled", ""],
      ["webapps", "show-toolbar", PWA, "showToolbar"],
      ["performance", "idle-memory-reclaim-enabled", MEMORY, "enabled"],
    ]
  ) {
    tests.push({
      name: `${routeName} ${id} saves and reloads`,
      fn: async () => {
        await route(`features/${routeName}`, `#${id}`);
        await toggle(`#${id}`, key, path || undefined);
        const expected = element<HTMLInputElement>(`#${id}`).checked;
        await route(`features/${routeName}`, `#${id}`);
        assertEquals(
          element<HTMLInputElement>(`#${id}`).checked,
          expected,
          "Value reloads",
        );
      },
    });
  }
  tests.push({
    name: "Panel width and side save and reload",
    fn: async () => {
      await route("features/sidebar", "#global-width");
      await input("#global-width", "450");
      await until(
        () => valueAt(PANEL, "globalWidth") === 450,
        "Panel width save",
      );
      await click('input[name="position"][value="start"]');
      await until(
        () => valueAt(PANEL, "position_start") === true,
        "Panel position save",
      );
      await route("features/sidebar", "#global-width");
      assertEquals(
        element<HTMLInputElement>("#global-width").value,
        "450",
        "Panel width reload",
      );
      assertEquals(
        valueAt(PANEL, "futureKey"),
        "keep",
        "Unexposed panel settings survive",
      );
    },
  });
  tests.push({
    name: "Memory fields commit, clamp and reload",
    fn: async () => {
      await route("features/performance", "#idle-memory-reclaim-enabled");
      if (
        !element<HTMLInputElement>("#idle-memory-reclaim-enabled").checked
      ) await click("#idle-memory-reclaim-enabled");
      for (
        const [id, key, value] of [["idle-threshold", "idleThresholdSec", 90], [
          "min-interval",
          "minIntervalSec",
          180,
        ], ["min-resident", "minResidentMB", 0]] as const
      ) {
        const selector = `#idle-memory-reclaim-${id}`;
        const before = valueAt(MEMORY, key);
        await input(selector, String(value));
        assertEquals(valueAt(MEMORY, key), before, "Typing is draft");
        element<HTMLInputElement>(selector).blur();
        await until(() => valueAt(MEMORY, key) === value, `${key} commits`);
        await route("features/performance", selector);
        assertEquals(
          element<HTMLInputElement>(selector).value,
          String(value),
          "Number reloads",
        );
      }
      await input("#idle-memory-reclaim-idle-threshold", "1");
      element<HTMLInputElement>("#idle-memory-reclaim-idle-threshold").blur();
      await until(
        () => valueAt(MEMORY, "idleThresholdSec") === 60,
        "Minimum clamps",
      );
      assertEquals(
        element<HTMLInputElement>("#idle-memory-reclaim-idle-threshold").value,
        "60",
        "Clamped number shown",
      );
      assertEquals(
        valueAt(MEMORY, "pollIntervalSec"),
        60,
        "Hidden memory preference preserved",
      );
    },
  });
  tests.push({
    name: "Workspaces failure is visible and retry preserves edit",
    fn: async () => {
      await route("features/workspaces", "#show-name");
      const before = valueAt(WORKSPACES, "showWorkspaceNameOnToolbar");
      faults.write = WORKSPACES;
      try {
        await click("#show-name");
        await until(
          () => document.querySelector('[role="alert"]'),
          "Save failure not shown",
        );
        globalThis.dispatchEvent(new Event("focus"));
        await pause();
        assertEquals(
          element<HTMLInputElement>("#show-name").checked,
          !before,
          "Failed edit retained",
        );
      } finally {
        faults.write = "";
      }
      await button("Retry");
      await until(
        () => valueAt(WORKSPACES, "showWorkspaceNameOnToolbar") === !before,
        "Retry did not save",
      );
    },
  });
  tests.push({
    name: "Unexposed workspace and PWA settings survive edits",
    fn: () => {
      assertEquals(
        valueAt(WORKSPACES, "futureKey"),
        "keep",
        "Workspace hidden data",
      );
      assertEquals(valueAt(PWA, "futureKey"), "keep", "PWA hidden data");
    },
  });
  for (
    const [path, key, selector] of [
      ["design", DESIGN, "#favicon-color"],
      ["workspaces", WORKSPACES, "#show-name"],
      ["sidebar", PANEL, "#global-width"],
      ["webapps", PWA, "#enable-pwa"],
    ]
  ) {
    tests.push({
      name: `${path} failed load disables editing without overwriting settings`,
      fn: async () => {
        await click('a[href="/overview/home"]');
        const before = prefs.get(key);
        faults.read = key;
        try {
          await click(`a[href="/features/${path}"]`);
          await until(
            () => document.querySelector("[role=alert]"),
            "Load error missing",
          );
          assert(
            element(selector).matches(":disabled"),
            "Editing enabled after failed load",
          );
          assertEquals(
            prefs.get(key),
            before,
            "Failed read overwrote configuration",
          );
        } finally {
          faults.read = "";
        }
        await route(`features/${path}`, selector);
      },
    });
  }
  await executeCases(tests);
}

export async function runAllTests() {
  await runFixture("http://localhost:5196/test/integration/index.html");
}
