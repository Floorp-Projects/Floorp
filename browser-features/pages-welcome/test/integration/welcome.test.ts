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
  pause,
  prefs,
  runFixture,
  until,
  writes,
} from "../../../../libs/ui/test/page-fixture.ts";
import { state } from "./bridge.ts";
const THEME = "layout.css.prefers-color-scheme.content-override";
const MODE = "floorp.releaseNotes.mode";
async function step(path: string, selector: string) {
  await input("nav select", path);
  await until(
    () => document.querySelector(selector),
    `Welcome ${path} did not render`,
  );
  await pause();
}
export async function runPageTests() {
  await until(
    () => document.querySelector("nav select"),
    "Welcome did not start",
    15000,
  );
  const tests: TestCase[] = [{
    name: "Welcome brand images and fonts load without saving initial choices",
    fn: async () => {
      await until(
        () =>
          [...document.querySelectorAll<HTMLImageElement>("header img")].every((
            img,
          ) => img.complete && img.naturalWidth > 0),
        "Brand image failed to load",
      );
      assert(
        document.querySelectorAll("header img").length > 0,
        "Brand images missing",
      );
      for (
        const [font, sample] of [["Inter", "Floorp"], ["Noto Sans JP", "設定"]]
      ) {
        const loaded = await document.fonts.load(`16px "${font}"`, sample);
        assert(
          loaded.length > 0 && loaded.every((face) => face.status === "loaded"),
          `${font} failed to load`,
        );
      }
      assertEquals(
        prefs.get("floorp.browser.welcome.page.shown"),
        true,
        "Shown marker",
      );
      assert(
        !writes.some((write) =>
          write.method === THEME || write.method === MODE
        ),
        "Initial render changed user choices",
      );
    },
  }];
  for (
    const [theme, value] of [["dark", 0], ["light", 1], ["system", 2]] as const
  ) {
    tests.push({
      name: `Welcome ${theme} theme persists across steps`,
      fn: async () => {
        await step("/customize", "#setup-engine");
        await click(`input[type=radio][value=${theme}]`);
        await until(
          () => prefs.get(THEME) === value,
          "Theme value did not save",
        );
        await step("/", "#setup-title");
        await step("/customize", "#setup-engine");
        assert(
          element<HTMLInputElement>(`input[type=radio][value=${theme}]`)
            .checked,
          "Theme selection was lost",
        );
      },
    });
  }
  tests.push({
    name: "Welcome theme failure retains previous selection",
    fn: async () => {
      faults.write = THEME;
      try {
        await click("input[type=radio][value=dark]");
        await until(
          () => document.querySelector("[role=alert]"),
          "Theme error missing",
        );
        assertEquals(prefs.get(THEME), 2, "Failed theme saved");
        assert(
          element<HTMLInputElement>("input[value=system]").checked,
          "Failed theme shown as saved",
        );
      } finally {
        faults.write = "";
      }
    },
  });
  for (const engine of ["duckduckgo", "bing", "google"]) {
    tests.push({
      name: `Welcome ${engine} search selection saves and reloads`,
      fn: async () => {
        await step("/customize", "#setup-engine");
        await input("#setup-engine", engine);
        assertEquals(state.engine, engine, "Search engine backend");
        await step("/", "#setup-title");
        await step("/customize", "#setup-engine");
        assertEquals(
          element<HTMLSelectElement>("#setup-engine").value,
          engine,
          "Search engine reload",
        );
      },
    });
  }
  tests.push({
    name: "Welcome rejected search selection stays unchanged",
    fn: async () => {
      state.fail = "engine";
      try {
        await input("#setup-engine", "bing");
        await until(
          () => document.querySelector("[role=alert]"),
          "Search error missing",
        );
        assertEquals(
          element<HTMLSelectElement>("#setup-engine").value,
          "google",
          "Rejected engine shown as selected",
        );
      } finally {
        state.fail = "";
      }
    },
  });
  tests.push({
    name: "Welcome language install failure does not apply locale",
    fn: async () => {
      await step("/localization", "#setup-language");
      state.fail = "install";
      try {
        await input("#setup-language", "en-GB");
        await until(
          () => document.querySelector("[role=alert]"),
          "Install error missing",
        );
        assertEquals(state.locale, "en-US", "Failed install applied");
      } finally {
        state.fail = "";
      }
    },
  });
  tests.push({
    name: "Welcome language installs then applies and reloads",
    fn: async () => {
      calls.length = 0;
      await input("#setup-language", "en-GB");
      await until(() => state.locale === "en-GB", "Locale not applied");
      assertEquals(
        calls.filter((call) => /LangPack|AppLocale/.test(call.method)).map((
          call,
        ) => call.method).join(","),
        "NRInstallLangPack,NRSetAppLocale",
        "Install before apply",
      );
      await step("/", "#setup-title");
      await step("/localization", "#setup-language");
      assertEquals(
        element<HTMLSelectElement>("#setup-language").value,
        "en-GB",
        "Locale reload",
      );
    },
  });
  tests.push({
    name: "Welcome system language keeps region",
    fn: async () => {
      await button("Use System Language");
      await until(
        () => state.locale === "en-US",
        "System locale did not use installed en-US",
      );
      assertEquals(
        element<HTMLSelectElement>("#setup-language").value,
        "en-US",
        "System locale selection",
      );
    },
  });
  tests.push({
    name: "Welcome rejected locale is not displayed as applied",
    fn: async () => {
      await input("#setup-language", "en-US");
      state.fail = "locale";
      try {
        await input("#setup-language", "en-GB");
        await until(
          () => document.querySelector("[role=alert]"),
          "Locale error missing",
        );
        assertEquals(
          element<HTMLSelectElement>("#setup-language").value,
          "en-US",
          "Rejected locale shown as selected",
        );
      } finally {
        state.fail = "";
      }
    },
  });
  for (const path of ["/features", "/hub", "/customize", "/support"]) {
    tests.push({
      name: `Welcome ${path} navigation renders content`,
      fn: async () => {
        await step(
          path,
          path === "/support" ? "input[value=support]" : "#welcome-content h2",
        );
        assert(
          element("#welcome-content").textContent!.trim().length > 100,
          "Blank welcome page",
        );
      },
    });
  }
  tests.push({
    name: "Welcome support choices remain draft until completion",
    fn: async () => {
      for (const mode of ["disabled", "blocking", "support"]) {
        await click(`input[type=radio][value=${mode}]`);
        await step("/customize", "#setup-engine");
        await step("/support", "input[value=support]");
        assert(
          element<HTMLInputElement>(`input[value=${mode}]`).checked,
          "Draft selection lost",
        );
        assert(!prefs.has(MODE), "Support preference saved before completion");
      }
    },
  });
  tests.push({
    name: "Welcome default browser reports failure and success",
    fn: async () => {
      await step("/finish", "#welcome-content h2");
      state.fail = "default";
      try {
        await button("Set as Default Browser");
        await until(
          () => document.querySelector("[role=alert]"),
          "Default browser failure not shown",
        );
      } finally {
        state.fail = "";
      }
      await button("Set as Default Browser");
      assert(
        calls.some((call) => call.method === "NRSetDefaultBrowser"),
        "Default browser API not called",
      );
    },
  });
  tests.push({
    name:
      "Welcome completion failure keeps page open; retry confirms preference",
    fn: async () => {
      faults.write = MODE;
      try {
        await button("Start using Floorp");
        await until(
          () => document.querySelector("[role=alert]"),
          "Completion failure missing",
        );
        assert(
          !calls.some((call) => call.method === "close"),
          "Closed after save failure",
        );
      } finally {
        faults.write = "";
      }
      await button("Start using Floorp");
      await until(
        () => calls.some((call) => call.method === "close"),
        "Completion did not close",
      );
      assertEquals(prefs.get(MODE), "support", "Support choice applied");
      assertEquals(
        prefs.get("floorp.releaseNotes.choiceConfirmed"),
        true,
        "Choice confirmed",
      );
      assert(
        calls.some((call) =>
          call.method === "open" && call.args[0] === "about:newtab"
        ),
        "New tab not opened",
      );
    },
  });
  await executeCases(tests);
}
export async function runAllTests() {
  await runFixture("http://localhost:5197/test/integration/index.html");
}
