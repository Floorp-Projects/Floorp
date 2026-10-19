// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { sidebarFiles } from "./helpers/resource-paths/fixture-files.mjs";
import { install, waitFor } from "./helpers/resource-paths/resource-path-fixture.ts";
import type { DevtoolsModule, Toolbox } from "./helpers/resource-paths/resource-path-types.ts";
const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs",
);
const { gDevTools } = require(
  "devtools/client/framework/devtools.js",
) as DevtoolsModule;
async function sidebarCase(compatible: boolean, index: number): Promise<void> {
  const original = "d".repeat(31) + "ef"[index],
    id = compatible
      ? original + "@chromium-extension"
      : "ordinary-sidebar-" + original + "@example.invalid";
  const owned = await install(sidebarFiles(compatible, true, original), id);
  let toolbox: Toolbox | undefined;
  try {
    const target = owned.win.gBrowser.addTab(
      owned.win.wrappedJSObject.__floorpRuntimeFixtureURL,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    owned.win.gBrowser.selectedTab = target;
    toolbox = await gDevTools.showToolboxForTab(target, {
      toolId: "webconsole",
      hostType: "bottom",
    });
    const command = await owned.control("control.html");
    await waitFor(async () => {
      try {
        return (await command({ method: "ready" })).available;
      } catch {
        return false;
      }
    }, "real devtools public listener readiness");
    const created = await command({ method: "create" });
    assertEquals(
      created.available,
      compatible,
      "ordinary Firefox public surface preserved",
    );
    if (compatible) {
      assertEquals(
        created.returned,
        "undefined",
        "height call is synchronous void",
      );
    }
    const paneId = await waitFor(
      () =>
        [...toolbox!.inspectorExtensionSidebars].find(([, v]) =>
          v.title === "Owned Height Oracle"
        )?.[0],
      "real registered sidebar",
    );
    await toolbox.selectTool("inspector");
    const inspector = toolbox.getPanel("inspector");
    inspector.sidebar.show(paneId);
    const element = await waitFor(
      () => inspector.panelDoc.getElementById(paneId),
      "actual native sidebar DOM",
    );
    const height = async (expected: number) =>
      await waitFor(
        () => Math.abs(element.getBoundingClientRect().height - expected) <= 1,
        "real sidebar height " + expected,
      );
    if (compatible) {
      await height(75);
      assertEquals(
        (await command({ method: "height", value: "120px" })).returned,
        "undefined",
        "subsequent call returns void",
      );
      await height(120);
    }
    for (const method of ["object", "expression", "page"]) {
      await command({ method });
      const expected = method === "object"
        ? "object-treeview"
        : method === "expression"
        ? "object-value-grip-view"
        : "extension-page";
      await waitFor(
        () =>
          (inspector.store.getState().extensionsSidebar[paneId] as {
            viewMode?: string;
          } | undefined)?.viewMode === expected,
        "actual content transition " + method,
      );
      if (compatible) {
        await height(120);
        assertEquals(
          element.style.height,
          "120px",
          "height persists across " + method,
        );
      }
    }
    if (compatible) {
      assertEquals(
        (await command({ method: "height", value: 42 })).errorName,
        "TypeError",
        "nonstring is rejected before state change",
      );
      await height(120);
    }
    const active = toolbox;
    await owned.cleanup();
    await waitFor(
      () => !active.inspectorExtensionSidebars.has(paneId),
      "native uninstall unregisters sidebar",
    );
    assert(
      !(paneId in inspector.store.getState().extensionsSidebar),
      "uninstall releases sidebar Redux height/content state",
    );
  } finally {
    try {
      await toolbox?.destroy();
    } finally {
      await owned.cleanup();
    }
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("ChromeDevtoolsSidebarHeightNative", [{
    name: "Chrome native sidebar height geometry and content-mode persistence",
    fn: () => sidebarCase(true, 0),
  }, {
    name: "ordinary Firefox native sidebar API remains unchanged",
    fn: () => sidebarCase(false, 1),
  }]);
}
