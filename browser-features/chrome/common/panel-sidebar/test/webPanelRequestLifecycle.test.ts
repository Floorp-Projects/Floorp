// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  panelSidebarConfig,
  panelSidebarData,
  selectedPanelId,
  setPanelSidebarConfig,
  setPanelSidebarData,
} from "../data/data.ts";
import { PanelNavigator } from "../panel-navigator.ts";
import { getWebPanelContentBrowser } from "../utils/web-panel-browser.ts";
import { observeWebPanelRequests } from "../utils/webRequest.ts";

function observerCount(): number {
  let count = 0;
  const observers = Services.obs.enumerateObservers("http-on-modify-request");
  while (observers.hasMoreElements()) {
    observers.getNext();
    count++;
  }
  return count;
}

async function waitFor(check: () => boolean, label: string): Promise<void> {
  for (let attempt = 0; attempt < 300; attempt++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  throw new Error(`Timed out waiting for HTTP observer lifecycle: ${label}`);
}

function testOwnerDisposal(): void {
  const baseline = observerCount();
  const owner = Object.assign(new EventTarget(), {
    closed: false,
    location: { href: "chrome://browser/content/browser.xhtml" },
    document: { querySelectorAll: () => [] },
  }) as unknown as Window;
  const dispose = observeWebPanelRequests(owner);
  try {
    assertEquals(observerCount(), baseline + 1, "parent owns one observer");
    owner.dispatchEvent(new Event("unload"));
    assertEquals(observerCount(), baseline, "window unload releases observer");
    dispose();
    dispose();
    assertEquals(observerCount(), baseline, "repeated cleanup is safe");
  } finally {
    dispose();
  }

  const child = Object.assign(new EventTarget(), {
    closed: false,
    location: {
      href: "chrome://browser/content/browser.xhtml?floorpWebPanelId=test",
    },
  }) as unknown as Window;
  // The URL guard also applies before the child's initialization sets its flag.
  observeWebPanelRequests(child)();
  assertEquals(observerCount(), baseline, "embedded child never registers");

  const disposeManually = observeWebPanelRequests(owner);
  disposeManually();
  owner.dispatchEvent(new Event("unload"));
  assertEquals(
    observerCount(),
    baseline,
    "root disposal before unload is safe",
  );
}

async function testPanelReuseAndRecreation(): Promise<void> {
  const controller = PanelNavigator.gPanelSidebar;
  assert(controller, "panel sidebar must be initialized");
  const config = panelSidebarConfig();
  const panels = panelSidebarData();
  const selected = selectedPanelId();
  const baseline = observerCount();
  const id = "web-panel-request-lifecycle-regression";
  try {
    setPanelSidebarConfig({ ...config, autoUnload: false });
    controller.closePanel();
    setPanelSidebarData([...panels, {
      id,
      type: "web",
      url: "about:blank",
      width: 400,
      userAgent: true,
      userContextId: 0,
      icon: undefined,
      zoomLevel: undefined,
      extensionId: undefined,
    }]);
    for (let cycle = 0; cycle < 3; cycle++) {
      controller.openPanel(id);
      await waitFor(
        () => !!getWebPanelContentBrowser(id)?.browserId,
        `panel ready on cycle ${cycle}`,
      );
      const browser = getWebPanelContentBrowser(id);
      assert(browser, "panel must have an inner content browser");
      assertEquals(
        observerCount(),
        baseline,
        "creating an embedded child must not register another observer",
      );
      controller.closePanel();
      assertEquals(selectedPanelId(), null, "hide clears panel selection");
      assertEquals(
        getWebPanelContentBrowser(id),
        browser,
        "ordinary hide preserves the content browser for reuse",
      );
      controller.openPanel(id);
      assertEquals(
        getWebPanelContentBrowser(id),
        browser,
        "show reuses the same hidden content browser",
      );
      assertEquals(observerCount(), baseline, "hide/show keeps observer count");
      controller.unloadPanel(id);
      await waitFor(() => !getWebPanelContentBrowser(id), "panel unload");
      assertEquals(
        observerCount(),
        baseline,
        "unload/recreation must not accumulate child observers",
      );
    }
  } finally {
    controller.unloadPanel(id);
    setPanelSidebarData(panels);
    setPanelSidebarConfig(config);
    if (selected) controller.openPanel(selected);
  }
}

async function testNativeWindowClose(): Promise<void> {
  const baseline = observerCount();
  const openWindow = (globalThis as unknown as { OpenBrowserWindow(): Window })
    .OpenBrowserWindow;
  let otherWindow: Window | null = null;
  try {
    for (let cycle = 0; cycle < 3; cycle++) {
      otherWindow = openWindow();
      await waitFor(
        () => observerCount() >= baseline + 1,
        `new parent observer on cycle ${cycle}`,
      );
      assertEquals(
        observerCount(),
        baseline + 1,
        "a second browser window adds exactly one owned observer",
      );
      otherWindow.close();
      otherWindow = null;
      await waitFor(
        () => observerCount() === baseline,
        `closed parent releases observer on cycle ${cycle}`,
      );
    }
  } finally {
    otherWindow?.close();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelRequestLifecycle.test.ts", [
    {
      name: "HTTP observer has an idempotent parent-window owner",
      fn: testOwnerDisposal,
    },
    {
      name:
        "panel hide/show reuses content and three recreations keep observer count",
      fn: testPanelReuseAndRecreation,
    },
    {
      name: "three native browser-window closes release HTTP observers",
      fn: testNativeWindowClose,
    },
  ]);
}
