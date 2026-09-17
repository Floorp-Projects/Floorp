// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const EXPECTED_LOOPBACK_ACTORS = [
  "NRSettings",
  "NRExperimemmt",
  "NRPanelSidebar",
  "NRTabManager",
  "NRSyncManager",
  "NRAppConstants",
  "NRRestartBrowser",
  "NRWorkspaces",
  "NRProgressiveWebApp",
  "NRPwaManager",
  "NRChromeModal",
  "NRProfileManager",
  "NRStartPage",
  "NRWelcomePage",
  "NRSearchEngine",
  "NRWebScraper",
  "NROSAutomotor",
  "NRI18n",
  "NRPluginStore",
  "NRKeyboardShortcutFocus",
  "NRMouseGestureScroll",
];

interface TestTab {
  linkedBrowser: XULBrowserElement;
}

interface TestGBrowser {
  selectedTab: TestTab;
  addTab(
    uri: string,
    options: { triggeringPrincipal: nsIPrincipal },
  ): TestTab;
  removeTab(tab: TestTab): void;
}

function sleep(milliseconds: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function waitForLoopbackGlobal(
  browser: XULBrowserElement,
): Promise<WindowGlobalParent | null> {
  const deadline = Date.now() + 10_000;
  while (Date.now() < deadline) {
    const global = browser.browsingContext?.currentWindowGlobal;
    if (global?.documentURI?.spec.startsWith("http://localhost:5181/")) {
      return global;
    }
    await sleep(50);
  }
  return null;
}

async function testActorsAllowTheirMatchedWebProcess(): Promise<void> {
  const testGBrowser = (window as unknown as { gBrowser?: TestGBrowser })
    .gBrowser;
  assert(
    testGBrowser !== undefined,
    "gBrowser should be available in a browser integration test",
  );

  const originalTab = testGBrowser.selectedTab;
  const tab = testGBrowser.addTab("http://localhost:5181/", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  testGBrowser.selectedTab = tab;

  try {
    const browser = tab.linkedBrowser;
    const global = await waitForLoopbackGlobal(browser);
    assert(global !== null, "the loopback test document should finish loading");

    const remoteType = browser.remoteType;
    assert(
      remoteType.startsWith("web"),
      `the loopback page should use a web remote type (got ${remoteType})`,
    );

    for (const actorName of EXPECTED_LOOPBACK_ACTORS) {
      const actor = global.getActor(actorName);
      assert(
        actor !== null,
        `${actorName} should be available in the matched web process`,
      );
    }

    let chromeStoreRejected = false;
    try {
      global.getActor("NRChromeWebStore");
    } catch {
      chromeStoreRejected = true;
    }
    assert(
      chromeStoreRejected,
      "NRChromeWebStore should remain unavailable outside its store origins",
    );
  } finally {
    if (tab !== originalTab) {
      testGBrowser.removeTab(tab);
      testGBrowser.selectedTab = originalTab;
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "matched actors instantiate in Firefox web remote types",
      fn: testActorsAllowTheirMatchedWebProcess,
    },
  ];
  await runTests("JSWindowActorRemoteType.test.mts", tests);
}
