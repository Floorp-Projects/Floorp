// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import type { StatusbarTestWindow } from "./types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const area = "nora-statusbar";
const expectedWidgets = ["screenshot-button", "fullscreen-button"];
const browserWindow = window as unknown as StatusbarTestWindow;

async function waitFor(check: () => boolean, label: string): Promise<void> {
  for (let attempt = 0; attempt < 200; attempt++) {
    if (check()) return;
    await new Promise((resolve) => browserWindow.setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for statusbar lifecycle: ${label}`);
}

function assertToolbarWidgets(win: StatusbarTestWindow, label: string): void {
  assertEquals(win.CustomizableUI.getAreaType(area), "toolbar", label);
  assert(win.CustomizableUI.areas.includes(area), `${label}: area is listed`);
  const toolbar = win.document.getElementById(area);
  assert(toolbar?.isConnected, `${label}: toolbar remains connected`);
  const placements = win.CustomizableUI.getWidgetIdsInArea(area);
  for (const id of expectedWidgets) {
    assert(placements.includes(id), `${label}: placement for ${id} survives`);
    assert(
      toolbar.querySelector(`[id="${id}"]`)?.isConnected,
      `${label}: real toolbar still contains ${id}`,
    );
  }
}

async function closeNativeWindow(win: StatusbarTestWindow): Promise<void> {
  let unloaded = false;
  const onUnload = () => unloaded = true;
  win.addEventListener("unload", onUnload, { once: true });
  try {
    win.close();
    await waitFor(() => win.closed && unloaded, "native unload completes");
    await new Promise((resolve) => browserWindow.setTimeout(resolve, 0));
  } finally {
    win.removeEventListener("unload", onUnload);
  }
}

async function testNativeOwnerClosePreservesArea(): Promise<void> {
  const hadPref = Services.prefs.prefHasUserValue("noraneko.statusbar.enable");
  const previous = Services.prefs.getBoolPref(
    "noraneko.statusbar.enable",
    false,
  );
  const originalPlacements = browserWindow.CustomizableUI.getWidgetIdsInArea(
    area,
  );
  const mainToolbar = document.getElementById(area);
  let first: StatusbarTestWindow | null = null;
  let survivor: StatusbarTestWindow | null = null;
  try {
    Services.prefs.setBoolPref("noraneko.statusbar.enable", true);
    first = browserWindow.OpenBrowserWindow();
    await waitFor(
      () =>
        !!first?.document.getElementById(area)?.querySelector(
          "#fullscreen-button",
        ),
      "first real window toolbar",
    );
    survivor = browserWindow.OpenBrowserWindow();
    await waitFor(
      () =>
        !!survivor?.document.getElementById(area)?.querySelector(
          "#fullscreen-button",
        ),
      "surviving real window toolbar",
    );
    assertToolbarWidgets(survivor, "before first owner close");
    await closeNativeWindow(first);
    assertToolbarWidgets(survivor, "after first owner close");
    assert(
      survivor.document.getElementById(area)!.getBoundingClientRect().height >
        0,
      "survivor's enabled toolbar remains visible",
    );

    await closeNativeWindow(survivor);
    assertToolbarWidgets(browserWindow, "remaining test browser");
    assertEquals(
      document.getElementById(area),
      mainToolbar,
      "native close preserves the remaining window's toolbar node",
    );
    assertEquals(
      JSON.stringify(browserWindow.CustomizableUI.getWidgetIdsInArea(area)),
      JSON.stringify(originalPlacements),
      "native close must not destroy process-wide saved placements",
    );
  } finally {
    if (first && !first.closed) first.close();
    if (survivor && !survivor.closed) survivor.close();
    if (hadPref) {
      Services.prefs.setBoolPref("noraneko.statusbar.enable", previous);
    } else {
      Services.prefs.clearUserPref("noraneko.statusbar.enable");
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("window-lifecycle.test.ts", [{
    name:
      "native window close preserves surviving toolbar widgets and global area",
    fn: testNativeOwnerClosePreservesArea,
  }]);
}
