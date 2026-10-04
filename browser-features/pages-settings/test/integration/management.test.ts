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
  expose,
  faults,
  input,
  json,
  pause,
  runFixture,
  until,
} from "../../../../libs/ui/test/page-fixture.ts";
import type { Panel } from "../../../chrome/common/panel-sidebar/utils/type.ts";
import { route } from "./settings.test.ts";
import { management } from "./management-bridge.ts";
const panels = () => json("floorp.panelSidebar.data").data as Panel[];
export async function runManagementTests() {
  await until(
    () => document.querySelector('a[href="/features/design"]'),
    "App did not start",
    15000,
  );
  const tests: TestCase[] = [{
    name: "Concurrent empty panel option requests all settle",
    fn: async () => {
      const data = await import("../../src/app/sidebar/dataManager.ts");
      for (
        const [name, get] of [["NRGetContainerContexts", data.getContainers], [
          "NRGetStaticPanels",
          data.getStaticPanels,
        ], ["NRGetExtensionPanels", data.getExtensionPanels]] as const
      ) {
        const original =
          Object.getOwnPropertyDescriptor(globalThis, name)!.value;
        expose(
          name,
          (callback: (data: string) => void) =>
            setTimeout(() => callback("[]"), 10),
        );
        try {
          const result = await Promise.race([
            Promise.all([get(), get()]),
            pause(1000).then(() => {
              throw new Error(`${name} left an empty response pending`);
            }),
          ]);
          assert(
            result.every((items) => items.length === 0),
            "Empty options changed",
          );
        } finally {
          expose(name, original);
        }
      }
    },
  }, {
    name: "Web panel editor saves every field and reloads",
    fn: async () => {
      await route("features/sidebar", "#enable-panel");
      await button("Add Panel");
      await until(
        () => document.querySelector("#panel-url"),
        "Panel editor not ready",
      );
      await input("#panel-url", "https://example.invalid/panel");
      await input(
        "#panel-icon",
        "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg'/%3E",
      );
      await input("#panel-width", "500");
      await input("#panel-zoomLevel", "1.25");
      await input("#panel-userContextId", "1");
      await click("input[name=userAgent]");
      faults.write = "floorp.panelSidebar.data";
      try {
        await button("Save");
        await until(
          () => document.querySelector("[role=dialog] [role=alert]"),
          "Panel failure not shown",
        );
        assertEquals(panels().length, 0, "Panel written after failure");
      } finally {
        faults.write = "";
      }
      await button("Save");
      await until(() => panels().length === 1, "Panel not created");
      const panel = panels()[0];
      assertEquals(panel.url, "https://example.invalid/panel", "Panel URL");
      assertEquals(panel.width, 500, "Panel width");
      assertEquals(panel.zoomLevel, 1.25, "Panel zoom");
      assertEquals(panel.userContextId, 1, "Panel container");
      assertEquals(panel.userAgent, true, "Panel user agent");
      assert(panel.icon?.startsWith("data:image/"), "Panel icon not saved");
      await route("features/sidebar", "#enable-panel");
      await click('button[aria-label="Edit Panel"]');
      await until(
        () => document.querySelector("#panel-width"),
        "Panel edit reload",
      );
      assertEquals(
        element<HTMLInputElement>("#panel-width").value,
        "500",
        "Saved width reloaded",
      );
      await input("#panel-width", "600");
      await button("Save");
      await until(() => panels()[0].width === 600, "Edited panel not saved");
    },
  }];
  for (
    const [type, selector, value] of [[
      "static",
      "#panel-url",
      "floorp//bookmarks",
    ], ["extension", "#panel-extensionId", "test@example.invalid"]]
  ) {
    tests.push({
      name: `${type} panel creates correct type and source`,
      fn: async () => {
        await route("features/sidebar", "#enable-panel");
        await button("Add Panel");
        await until(
          () => document.querySelector("#panel-type"),
          "Panel modal not ready",
        );
        await input("#panel-type", type);
        await input(selector, value);
        await button("Save");
        await until(() =>
          panels().some((panel) =>
            panel.type === type &&
            (panel.type === "extension" ? panel.extensionId : panel.url) ===
              value
          ), "Panel type not saved");
      },
    });
  }
  tests.push({
    name: "Panel deletion requires confirmation and preserves other panels",
    fn: async () => {
      await route("features/sidebar", "#enable-panel");
      const count = panels().length;
      await click('button[aria-label="Delete"]');
      assertEquals(panels().length, count, "Deleted before confirmation");
      await button("Cancel");
      assertEquals(panels().length, count, "Cancelled deletion changed data");
      await click('button[aria-label="Delete"]');
      await button("Delete");
      await until(() => panels().length === count - 1, "Panel not deleted");
    },
  });
  tests.push({
    name: "PWA rename fails visibly, retries and reloads",
    fn: async () => {
      await route("features/webapps", "#enable-pwa");
      await button("Rename App");
      await input("[role=dialog] input", "Renamed app");
      management.fail = "rename";
      try {
        await button("Rename");
        await until(
          () => document.querySelector("[role=dialog] [role=alert]"),
          "Rename failure not shown",
        );
        assertEquals(
          management.apps.test.name,
          "Test app",
          "Failed rename applied",
        );
      } finally {
        management.fail = "";
      }
      await button("Rename");
      await until(
        () => !document.querySelector("[role=dialog]"),
        "Rename modal not closed",
      );
      await route("features/webapps", "#enable-pwa");
      await until(
        () => document.body.textContent!.includes("Renamed app"),
        "Renamed app not reloaded",
      );
    },
  });
  tests.push({
    name: "PWA container selection applies and reloads",
    fn: async () => {
      await button("Container");
      await input("[role=dialog] select", "1");
      await button("Save");
      assertEquals(
        management.apps.test.userContextId,
        1,
        "Container not saved",
      );
      await route("features/webapps", "#enable-pwa");
      await button("Container");
      assertEquals(
        element<HTMLSelectElement>("[role=dialog] select").value,
        "1",
        "Container not reloaded",
      );
      await button("Cancel");
    },
  });
  tests.push({
    name:
      "native PWA container rejection explains preserved session without changing data",
    fn: async () => {
      await button("Container");
      await input("[role=dialog] select", "0");
      management.fail = "native-context";
      try {
        await button("Save");
        await until(
          () =>
            document.querySelector("[role=dialog] [role=alert]")?.textContent
              ?.includes("current login and site data have been kept"),
          "Native container restriction was not explained",
        );
        assertEquals(
          management.apps.test.userContextId,
          1,
          "Rejected native change modified the active container",
        );
      } finally {
        management.fail = "";
      }
      await button("Cancel");
    },
  });
  tests.push({
    name: "PWA uninstall confirmation and cancellation",
    fn: async () => {
      await button("Uninstall App");
      await button("Cancel");
      assert(management.apps.test, "Uninstalled after cancel");
      await button("Uninstall App");
      management.fail = "uninstall";
      try {
        await button("Uninstall");
        await until(
          () => document.querySelector("[role=dialog] [role=alert]"),
          "Uninstall veto was not shown",
        );
        assert(management.apps.test, "Rejected uninstall removed app data");
      } finally {
        management.fail = "";
      }
      await button("Uninstall");
      assert(!management.apps.test, "PWA not uninstalled");
      await route("features/webapps", "#enable-pwa");
      assert(
        !document.body.textContent!.includes("Renamed app"),
        "Removed PWA still listed",
      );
      assert(
        calls.some((call) =>
          call.method === "NRUninstallSsb" && call.args[0] === "test"
        ),
        "Wrong app uninstall",
      );
    },
  });
  tests.push({
    name: "Floorp OS failed enable, enable, disable and reload",
    fn: async () => {
      await route("features/floorp-os", "main button");
      await pause();
      management.fail = "os";
      try {
        await button("Enable Floorp OS");
        assert(!management.osEnabled, "Failed enable changed OS state");
        await until(
          () => document.body.textContent!.includes("Injected enable failure"),
          "OS failure missing",
        );
      } finally {
        management.fail = "";
      }
      await button("Enable Floorp OS");
      await until(() => management.osEnabled, "OS not enabled");
      await route("features/floorp-os", "main button");
      await pause();
      await button("Disable Floorp OS");
      await until(() => !management.osEnabled, "OS not disabled");
    },
  });
  tests.push({
    name: "Workspace initialization calls the actor only after confirmation",
    fn: async () => {
      await route("features/workspaces", "#enable-workspaces");
      const count = () =>
        calls.filter((call) => call.method === "NRInitializeWorkspaces").length;
      const before = count();
      await button("Initialize workspaces");
      await button("Cancel");
      assertEquals(count(), before, "Cancelled initialization called actor");
      await button("Initialize workspaces");
      await button("Initialize");
      await until(
        () => count() === before + 1,
        "Initialization did not reach actor",
      );
    },
  });
  await executeCases(tests);
}
export async function runAllTests() {
  await runFixture(
    "http://localhost:5196/test/integration/index.html?suite=management",
  );
}
