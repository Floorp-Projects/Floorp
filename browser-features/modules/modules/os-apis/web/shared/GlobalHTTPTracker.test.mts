// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  runTests,
  type TestCase,
} from "../../../../../chrome/test/utils/test_harness.ts";

interface TrackerModule {
  GlobalHTTPTracker: {
    activeRequests: Map<number, Set<nsIRequest>>;
  };
}

interface TestTab {
  linkedBrowser: XULBrowserElement;
}

interface TestGBrowser {
  addTab(
    uri: string,
    options: { triggeringPrincipal: nsIPrincipal },
  ): TestTab;
  removeTab(tab: TestTab, options?: { animate?: boolean }): void;
}

function sleep(milliseconds: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

// Regression test for KAN-13: entries of a closed tab must be dropped when
// its browsing context is discarded, otherwise every HTTP channel a closed
// tab ever opened stays reachable from the tracker for the whole session.
async function testClosedTabEntriesAreDropped(): Promise<void> {
  // Use the packaged singleton, which is the instance the browser actually
  // keeps alive, rather than a bundled copy of the module.
  const { GlobalHTTPTracker: tracker } = ChromeUtils.importESModule(
    "resource://noraneko/modules/os-apis/web/shared/GlobalHTTPTracker.sys.mjs",
  ) as unknown as TrackerModule;

  const testGBrowser = (window as unknown as { gBrowser?: TestGBrowser })
    .gBrowser;
  assert(
    testGBrowser !== undefined,
    "gBrowser should be available in a browser integration test",
  );

  const tab = testGBrowser.addTab("about:blank", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  let tabRemoved = false;
  let bcid: number | undefined;
  let retained = false;
  try {
    bcid = tab.linkedBrowser.browsingContext?.id;
    assert(bcid !== undefined, "the new tab should have a browsing context");

    // Stand in for a request the tab left behind (e.g. one still pending when
    // the tab closed) so the test does not depend on network access.
    tracker.activeRequests.set(
      bcid,
      new Set([{ name: "kan13-test" } as unknown as nsIRequest]),
    );

    testGBrowser.removeTab(tab, { animate: false });
    tabRemoved = true;

    const deadline = Date.now() + 10_000;
    while (tracker.activeRequests.has(bcid) && Date.now() < deadline) {
      await sleep(50);
    }
    retained = tracker.activeRequests.has(bcid);
  } finally {
    if (bcid !== undefined) tracker.activeRequests.delete(bcid);
    if (!tabRemoved) testGBrowser.removeTab(tab, { animate: false });
  }

  assert(
    !retained,
    "closing a tab should drop its tracked requests once its browsing context is discarded",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "closed tab entries are dropped on browsing-context-discarded",
      fn: testClosedTabEntriesAreDropped,
    },
  ];
  await runTests("GlobalHTTPTracker.test.mts", tests);
}
