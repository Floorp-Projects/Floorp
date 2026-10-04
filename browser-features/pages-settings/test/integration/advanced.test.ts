// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  button,
  calls,
  click,
  element,
  executeCases,
  faults,
  input,
  json,
  pause,
  prefs,
  runFixture,
  until,
  valueAt,
} from "../../../../libs/ui/test/page-fixture.ts";
import { route, toggle } from "./settings.test.ts";
const GESTURE = "floorp.mousegesture.config";
const SHORTCUT = "floorp.keyboardshortcut.config";
const DESIGN = "floorp.design.configs";
const chromeExtrasFields = [
  ["autohide-tab", "autohideTab"],
  ["autohide-navbar", "autohideNavbar"],
  ["autohide-sidebar", "autohideSidebar"],
  ["autohide-back-button", "autohideBackButton"],
  ["autohide-forward-button", "autohideForwardButton"],
  ["autohide-page-action", "autohidePageAction"],
  ["hidden-tab-icon", "hiddenTabIcon"],
  ["hidden-tabbar", "hiddenTabbar"],
  ["hidden-navbar", "hiddenNavbar"],
  ["hidden-sidebar-header", "hiddenSidebarHeader"],
  ["hidden-urlbar-iconbox", "hiddenUrlbarIconbox"],
  ["hidden-bookmarkbar-icon", "hiddenBookmarkbarIcon"],
  ["hidden-bookmarkbar-label", "hiddenBookmarkbarLabel"],
  ["hidden-disabled-menu", "hiddenDisabledMenu"],
  ["icon-disabled", "iconDisabled"],
  ["icon-menu", "iconMenu"],
  ["centered-tab", "centeredTab"],
  ["centered-urlbar", "centeredUrlbar"],
  ["centered-bookmarkbar", "centeredBookmarkbar"],
  ["url-view-move-icon-to-left", "urlViewMoveIconToLeft"],
  ["url-view-go-button-when-typing", "urlViewGoButtonWhenTyping"],
  ["url-view-always-show-page-actions", "urlViewAlwaysShowPageActions"],
  ["tabbar-as-titlebar", "tabbarAsTitlebar"],
  ["tabbar-one-liner", "tabbarOneLiner"],
  ["sidebar-overlap", "sidebarOverlap"],
] as const;
async function chromeExtras() {
  await route("features/design", "#favicon-color");
  await button("Open UI extension settings");
  await until(
    () => document.querySelector("#autohide-tab"),
    "UI extension settings did not open",
  );
  await until(
    () => !document.querySelector("fieldset[disabled]"),
    "UI extension settings are still loading",
  );
}
async function gesture() {
  await route("features/gesture", '[data-setting="mouse-gesture-enabled"]');
}
export async function runAdvancedTests() {
  await until(
    () => document.querySelector('a[href="/features/design"]'),
    "App did not start",
    15000,
  );
  const tests: TestCase[] = [];
  tests.push({
    name: "UI extension settings are available with Proton selected",
    fn: async () => {
      await route("features/design", "#favicon-color");
      await click('input[name="design"][value="proton"]');
      await until(
        () => valueAt(DESIGN, "globalConfigs.userInterface") === "proton",
        "Proton selection did not save",
      );
      await chromeExtras();
      await route("features/design", "#favicon-color");
      await click('input[name="design"][value="lepton"]');
      await until(
        () => valueAt(DESIGN, "globalConfigs.userInterface") === "lepton",
        "Lepton selection did not restore",
      );
    },
  });
  for (const [id, key] of chromeExtrasFields) {
    tests.push({
      name: `UI extension ${key} saves and reloads`,
      fn: async () => {
        const path = `uiCustomization.chromeExtras.${key}`;
        await chromeExtras();
        const expected = !element<HTMLInputElement>(`#${id}`).checked;
        await toggle(`#${id}`, DESIGN, path);
        await chromeExtras();
        assertEquals(
          element<HTMLInputElement>(`#${id}`).checked,
          expected,
          `${key} did not reload from the design config`,
        );
      },
    });
  }
  tests.push({
    name: "Rapid UI extension changes preserve both settings",
    fn: async () => {
      await chromeExtras();
      const tab = element<HTMLInputElement>("#autohide-tab");
      const navbar = element<HTMLInputElement>("#autohide-navbar");
      const expectedTab = !tab.checked;
      const expectedNavbar = !navbar.checked;
      tab.click();
      navbar.click();
      await until(
        () =>
          valueAt(DESIGN, "uiCustomization.chromeExtras.autohideTab") ===
            expectedTab &&
          valueAt(DESIGN, "uiCustomization.chromeExtras.autohideNavbar") ===
            expectedNavbar,
        "Rapid changes did not both save",
      );
      await chromeExtras();
      assertEquals(
        element<HTMLInputElement>("#autohide-tab").checked,
        expectedTab,
        "Tab setting survived reload",
      );
      assertEquals(
        element<HTMLInputElement>("#autohide-navbar").checked,
        expectedNavbar,
        "Navbar setting survived reload",
      );
      assertEquals(json(DESIGN).futureKey, "keep", "Unrelated config survived");
    },
  });
  tests.push({
    name: "UI extension load failure disables editing and can retry",
    fn: async () => {
      await route("features/design", "#favicon-color");
      const before = prefs.get(DESIGN);
      faults.read = DESIGN;
      try {
        await button("Open UI extension settings");
        await until(
          () => document.querySelector('[role="alert"]'),
          "Load failure was not shown",
        );
        assert(
          element("#autohide-tab").matches(":disabled"),
          "Editing stayed enabled after a failed load",
        );
        assertEquals(prefs.get(DESIGN), before, "Failed load changed config");
      } finally {
        faults.read = "";
      }
      await button("Retry");
      await until(
        () => !element("#autohide-tab").matches(":disabled"),
        "Retry did not enable editing",
      );
    },
  });
  tests.push({
    name: "UI extension save failure is visible and retry preserves edit",
    fn: async () => {
      await chromeExtras();
      const before = valueAt(
        DESIGN,
        "uiCustomization.chromeExtras.hiddenTabbar",
      );
      faults.write = DESIGN;
      try {
        await click("#hidden-tabbar");
        await until(
          () => document.querySelector('[role="alert"]'),
          "Save failure was not shown",
        );
        assertEquals(
          element<HTMLInputElement>("#hidden-tabbar").checked,
          !before,
          "Failed edit remains visible",
        );
      } finally {
        faults.write = "";
      }
      await button("Retry");
      await until(
        () =>
          valueAt(DESIGN, "uiCustomization.chromeExtras.hiddenTabbar") ===
            !before,
        "Retry did not save the edit",
      );
    },
  });
  tests.push({
    name: "Tab sleep exclusion toggles and adds/removes patterns",
    fn: async () => {
      const key = "floorp.tabs.sleep.exclusion";
      await route("features/design", "#tab-sleep-exclusion-enabled");
      await toggle("#tab-sleep-exclusion-enabled", key, "enabled");
      await toggle("#tab-sleep-exclusion-enabled", key, "enabled");
      await input(
        'input[aria-label="Exclusion Patterns"]',
        "  *.example.test  ",
      );
      await button("Add");
      await until(
        () => (json(key).patterns as string[]).includes("*.example.test"),
        "Pattern not saved",
      );
      await route("features/design", "#tab-sleep-exclusion-enabled");
      assert(
        document.body.textContent!.includes("*.example.test"),
        "Pattern not reloaded",
      );
      await click('button[aria-label="Remove"]');
      assertEquals(
        (json(key).patterns as string[]).length,
        0,
        "Pattern removed",
      );
    },
  });
  tests.push({
    name: "Gesture enabled state saves and reloads",
    fn: async () => {
      await gesture();
      await toggle(
        '[data-setting="mouse-gesture-enabled"]',
        "floorp.mousegesture.enabled",
      );
      await gesture();
      assert(
        !element<HTMLInputElement>('[data-setting="mouse-gesture-enabled"]')
          .checked,
        "Enabled reload",
      );
      await toggle(
        '[data-setting="mouse-gesture-enabled"]',
        "floorp.mousegesture.enabled",
      );
    },
  });
  for (
    const [label, path] of [
      ["Enable rocker gestures", "rockerGesturesEnabled"],
      ["Show gesture trail", "showTrail"],
      ["Show gesture label", "showLabel"],
    ]
  ) {
    tests.push({
      name: `Gesture ${path} saves and reloads`,
      fn: async () => {
        await gesture();
        await toggle(`input[aria-label="${label}"]`, GESTURE, path);
        await gesture();
        assertEquals(
          element<HTMLInputElement>(`input[aria-label="${label}"]`).checked,
          valueAt(GESTURE, path),
          "Gesture reload",
        );
        await toggle(`input[aria-label="${label}"]`, GESTURE, path);
      },
    });
  }
  tests.push({
    name: "Gesture wheel toggle saves and reloads",
    fn: async () => {
      await gesture();
      await toggle(
        '[data-setting="mouse-gesture-wheel-enabled"]',
        GESTURE,
        "wheelGesturesEnabled",
      );
      await gesture();
      assert(
        !element<HTMLInputElement>(
          '[data-setting="mouse-gesture-wheel-enabled"]',
        ).checked,
        "Wheel reload",
      );
      await toggle(
        '[data-setting="mouse-gesture-wheel-enabled"]',
        GESTURE,
        "wheelGesturesEnabled",
      );
    },
  });
  tests.push({
    name: "Gesture sliders each save and reload",
    fn: async () => {
      await gesture();
      for (
        const [index, path] of [
          "sensitivity",
          "contextMenu.minDistance",
          "contextMenu.preventionTimeout",
          "trailWidth",
        ].entries()
      ) {
        const slider =
          document.querySelectorAll<HTMLElement>("[role=slider]")[index];
        assert(slider, `Slider ${index} missing`);
        const before = Number(slider.getAttribute("aria-valuenow"));
        slider.focus();
        slider.dispatchEvent(
          new KeyboardEvent("keydown", {
            key: "ArrowRight",
            code: "ArrowRight",
            bubbles: true,
          }),
        );
        await until(
          () => Number(valueAt(GESTURE, path)) > before,
          `${path} slider did not save`,
        );
        const saved = valueAt(GESTURE, path);
        await gesture();
        assertEquals(
          Number(
            document.querySelectorAll("[role=slider]")[index].getAttribute(
              "aria-valuenow",
            ),
          ),
          saved,
          "Slider reload",
        );
      }
    },
  });
  tests.push({
    name: "Gesture trail color and four rocker/wheel actions persist",
    fn: async () => {
      await gesture();
      await input("#gesture-trail-color-text", "#123456");
      assertEquals(valueAt(GESTURE, "trailColor"), "#123456", "Trail color");
      for (
        const [index, path] of [
          "rockerActions.leftRight",
          "rockerActions.rightLeft",
          "wheelActions.scrollUp",
          "wheelActions.scrollDown",
        ].entries()
      ) {
        const select =
          document.querySelectorAll<HTMLSelectElement>("main select")[index];
        const next = [...select.options].find((option) =>
          !option.disabled && option.value !== select.value
        )!;
        select.dataset.testSelect = String(index);
        await input(`[data-test-select="${index}"]`, next.value);
        assertEquals(valueAt(GESTURE, path), next.value, `${path} save`);
        await gesture();
        assertEquals(
          document.querySelectorAll<HTMLSelectElement>("main select")[index]
            .value,
          next.value,
          `${path} reload`,
        );
      }
      assertEquals(
        element<HTMLInputElement>("#gesture-trail-color-text").value,
        "#123456",
        "Color reload",
      );
    },
  });
  tests.push({
    name: "Gesture add/edit/delete action and save failure",
    fn: async () => {
      await gesture();
      await button("Add Gesture");
      await until(
        () => document.querySelector("[role=dialog] select"),
        "Gesture editor missing",
      );
      const select = element<HTMLSelectElement>("[role=dialog] select");
      const action = [...select.options].find((option) =>
        option.value && !option.disabled
      )!.value;
      await input("[role=dialog] select", action);
      await button("↖");
      await button("↘");
      faults.write = GESTURE;
      try {
        await button("Save");
        await until(
          () => document.querySelector("[role=dialog] [role=alert]"),
          "Gesture save failure missing",
        );
        assert(
          document.querySelector("[role=dialog]"),
          "Editor closed on failure",
        );
      } finally {
        faults.write = "";
      }
      await button("Save");
      await until(
        () => !document.querySelector("[role=dialog]"),
        "Gesture editor did not close",
      );
      const actions = json(GESTURE).actions as {
        action: string;
        pattern: string[];
      }[];
      assert(
        actions.some((item) =>
          item.action === action && item.pattern.join() === "upLeft,downRight"
        ),
        "Gesture not saved",
      );
      await gesture();
      assert(document.body.textContent!.includes("↖↘"), "Gesture not reloaded");
      await click('tbody tr:last-child button[title="Edit"]');
      await button("↑");
      await button("Save");
      await until(
        () =>
          (json(GESTURE).actions as { pattern: string[] }[]).some((item) =>
            item.pattern.join() === "upLeft,downRight,up"
          ),
        "Edited gesture not saved",
      );
      await gesture();
      await click('tbody tr:last-child button[title="Delete"]');
      await until(
        () =>
          !(json(GESTURE).actions as { pattern: string[] }[]).some((item) =>
            item.pattern.join() === "upLeft,downRight,up"
          ),
        "Gesture not deleted",
      );
    },
  });
  tests.push({
    name: "Keyboard enabled state saves and reloads",
    fn: async () => {
      await route("features/shortcuts", "#enable-shortcuts");
      await toggle("#enable-shortcuts", "floorp.keyboardshortcut.enabled");
      await route("features/shortcuts", "#enable-shortcuts");
      assert(
        !element<HTMLInputElement>("#enable-shortcuts").checked,
        "Shortcut enable reload",
      );
      await toggle("#enable-shortcuts", "floorp.keyboardshortcut.enabled");
    },
  });
  tests.push({
    name:
      "Keyboard shortcut editor records modifiers, retries failed save, reloads and deletes",
    fn: async () => {
      await route("features/shortcuts", "#enable-shortcuts");
      await click("tbody tr:first-child button");
      await until(
        () => document.querySelector("[role=dialog] input[type=text]"),
        "Shortcut editor missing",
      );
      for (
        const checkbox of document.querySelectorAll<HTMLInputElement>(
          "[role=dialog] input[type=checkbox]",
        )
      ) {
        if (!checkbox.checked) {
          checkbox.click();
          await pause();
        }
      }
      element<HTMLInputElement>("[role=dialog] input[type=text]").focus();
      await pause();
      globalThis.dispatchEvent(
        new KeyboardEvent("keydown", { key: "F9", code: "F9", bubbles: true }),
      );
      await pause();
      faults.write = SHORTCUT;
      try {
        await button("Save");
        await until(
          () => document.querySelector("[role=dialog] [role=alert]"),
          "Shortcut failure missing",
        );
      } finally {
        faults.write = "";
      }
      await button("Save");
      await until(
        () => !document.querySelector("[role=dialog]"),
        "Shortcut editor not closed",
      );
      const shortcuts = json(SHORTCUT).shortcuts as Record<
        string,
        { key: string; modifiers: Record<string, boolean> }
      >;
      const found = Object.entries(shortcuts).find(([, shortcut]) =>
        shortcut.key === "F9"
      );
      assert(found, "Recorded key missing");
      assert(
        Object.values(found[1].modifiers).every(Boolean),
        "Modifiers did not save",
      );
      await route("features/shortcuts", "#enable-shortcuts");
      assert(
        element("tbody tr:first-child").textContent!.includes("F9"),
        "Shortcut reload",
      );
      await click("tbody tr:first-child button:nth-child(2)");
      await until(
        () => !(json(SHORTCUT).shortcuts as Record<string, unknown>)[found[0]],
        "Shortcut not deleted",
      );
    },
  });
  tests.push({
    name: "Release notes modes apply and reload",
    fn: async () => {
      await route("about/updates", "input[name=release-notes-mode]");
      for (const mode of ["disabled", "support", "blocking"]) {
        await click(`input[name=release-notes-mode][value=${mode}]`);
        assertEquals(
          prefs.get("floorp.releaseNotes.mode"),
          mode,
          "Release notes save",
        );
        assertEquals(
          prefs.get("floorp.releaseNotes.choiceConfirmed"),
          true,
          "Consent saved",
        );
        await route("about/updates", "input[name=release-notes-mode]");
        assert(
          element<HTMLInputElement>(`input[value=${mode}]`).checked,
          "Release notes reload",
        );
      }
    },
  });
  tests.push({
    name: "Experiment participation policies apply and reload",
    fn: async () => {
      await route("about/updates", "main select");
      for (const policy of ["always", "never", "default"]) {
        await input("main select", policy);
        assertEquals(
          prefs.get("floorp.experiments.participationPolicy"),
          policy,
          "Policy save",
        );
        await route("about/updates", "main select");
        assertEquals(
          element<HTMLSelectElement>("main select").value,
          policy,
          "Policy reload",
        );
      }
      assert(
        calls.some((call) => call.method === "experiments.init"),
        "Policy was not applied to experiments",
      );
    },
  });
  tests.push({
    name: "Account sync and profile manager links call browser actions",
    fn: async () => {
      await route("features/accounts", "main a");
      for (
        const [text, target] of [[
          "Manage Firefox Feature Sync",
          "about:preferences#sync",
        ], ["Open Profile Manager", "about:profiles"]]
      ) {
        const link = [...document.querySelectorAll<HTMLAnchorElement>("main a")]
          .find((link) => link.textContent?.trim() === text);
        assert(link, `Missing ${text}`);
        link.click();
        await pause();
        assert(
          calls.some((call) =>
            call.method === "NRAddTab" && call.args[0] === target
          ),
          `${target} not opened`,
        );
      }
      const folder = [...document.querySelectorAll<HTMLAnchorElement>("main a")]
        .find((link) =>
          link.textContent?.trim() === "Open Profile Save Location"
        );
      assert(folder, "Profile folder link missing");
      folder.click();
      await pause();
      assert(
        calls.some((call) => call.method === "NROpenCurrentProfileDirectory"),
        "Profile folder action not called",
      );
    },
  });
  await executeCases(tests);
}
export async function runAllTests() {
  await runFixture(
    "http://localhost:5196/test/integration/index.html?suite=advanced",
  );
}
