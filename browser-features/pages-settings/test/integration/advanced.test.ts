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
const leptonFields = [
  ["autohide-tab", "autohide.tab"],
  ["autohide-navbar", "autohide.navbar"],
  ["autohide-sidebar", "autohide.sidebar"],
  ["autohide-back-button", "autohide.back_button"],
  ["autohide-forward-button", "autohide.forward_button"],
  ["autohide-page-action", "autohide.page_action"],
  ["hidden-tab-icon", "hidden.tab_icon"],
  ["hidden-tabbar", "hidden.tabbar"],
  ["hidden-navbar", "hidden.navbar"],
  ["hidden-sidebar-header", "hidden.sidebar_header"],
  ["hidden-urlbar-iconbox", "hidden.urlbar_iconbox"],
  ["hidden-bookmarkbar-icon", "hidden.bookmarkbar_icon"],
  ["hidden-bookmarkbar-label", "hidden.bookmarkbar_label"],
  ["hidden-disabled-menu", "hidden.disabled_menu"],
  ["icon-disabled", "icon.disabled"],
  ["icon-menu", "icon.menu"],
  ["centered-tab", "centered.tab"],
  ["centered-urlbar", "centered.urlbar"],
  ["centered-bookmarkbar", "centered.bookmarkbar"],
  ["url-view-move-icon-to-left", "urlView.move_icon_to_left"],
  ["url-view-go-button-when-typing", "urlView.go_button_when_typing"],
  ["url-view-always-show-page-actions", "urlView.always_show_page_actions"],
  ["tabbar-as-titlebar", "tabbar.as_titlebar"],
  ["tabbar-one-liner", "tabbar.one_liner"],
  ["sidebar-overlap", "sidebar.overlap"],
];
async function lepton() {
  await route("features/design", "#favicon-color");
  await button("Configure Lepton");
  await until(
    () => document.querySelector("#autohide-tab"),
    "Lepton not opened",
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
  for (const [id, pref] of leptonFields) {
    tests.push({
      name: `Lepton ${pref} saves and reloads`,
      fn: async () => {
        await lepton();
        await toggle(`#${id}`, `userChrome.${pref}`);
        await lepton();
        await until(
          () => element<HTMLInputElement>(`#${id}`).checked,
          "Lepton selection did not reload",
        );
      },
    });
  }
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
