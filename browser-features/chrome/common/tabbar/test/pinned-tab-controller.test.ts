// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { PinnedTabController } from "../multirow-tabbar/pinned-tab-controller.ts";

type TestTab = HTMLDivElement & { closing?: boolean };

type Fixture = {
  controller: PinnedTabController;
  destination: HTMLDivElement;
  host: HTMLDivElement;
  source: HTMLDivElement;
  unpinnedDestinationTab: TestTab;
};

function createTab(pinned: boolean): TestTab {
  const tab = document!.createElement("div") as TestTab;
  tab.classList.add("tabbrowser-tab");
  if (pinned) {
    tab.setAttribute("pinned", "true");
  }
  return tab;
}

function createFixture(): Fixture {
  const host = document!.createElement("div");
  const source = document!.createElement("div");
  const destination = document!.createElement("div");
  const unpinnedDestinationTab = createTab(false);

  host.hidden = true;
  destination.append(unpinnedDestinationTab);
  host.append(source, destination);
  const documentElement = document?.documentElement;
  assert(documentElement, "browser document should have a document element");
  documentElement.append(host);

  return {
    controller: new PinnedTabController(
      () => destination as unknown as XULElement,
    ),
    destination,
    host,
    source,
    unpinnedDestinationTab,
  };
}

async function withFixture(
  task: (fixture: Fixture) => void | Promise<void>,
): Promise<void> {
  const fixture = createFixture();
  try {
    await task(fixture);
  } finally {
    fixture.host.remove();
  }
}

function assertIgnored(
  tab: TestTab,
  expectedParent: Element | null,
  message: string,
): void {
  assertEquals(tab.parentElement, expectedParent, `${message}: parent`);
  assertEquals(
    tab.hasAttribute("newPin"),
    false,
    `${message}: newPin should not be stamped`,
  );
}

async function testMigratesLivePinnedDirectChild(): Promise<void> {
  await withFixture(
    ({ controller, destination, source, unpinnedDestinationTab }) => {
      const tab = createTab(true);
      source.append(tab);

      controller.migratePinnedTabs(destination, source.childNodes, source);

      assertEquals(
        tab.parentElement,
        destination,
        "live pinned tab should move",
      );
      assertEquals(
        destination.firstElementChild,
        tab,
        "pinned tab should be placed before the first unpinned tab",
      );
      assertEquals(
        destination.lastElementChild,
        unpinnedDestinationTab,
        "the existing unpinned tab should retain its relative position",
      );
      assertEquals(
        tab.getAttribute("newPin"),
        "true",
        "migrated pinned tab should be stamped",
      );
    },
  );
}

async function testIgnoresConnectedWrongParent(): Promise<void> {
  await withFixture(({ controller, destination, host, source }) => {
    const otherParent = document!.createElement("div");
    const tab = createTab(true);
    host.append(otherParent);
    source.append(tab);
    otherParent.append(tab);

    controller.migratePinnedTabs(destination, [tab], source);

    assertIgnored(tab, otherParent, "connected wrong-parent tab");
  });
}

async function testIgnoresDisconnectedPinnedTab(): Promise<void> {
  await withFixture(({ controller, destination, source }) => {
    const tab = createTab(true);

    controller.migratePinnedTabs(destination, [tab], source);

    assertIgnored(tab, null, "disconnected pinned tab");
  });
}

async function testIgnoresUnpinnedSourceChild(): Promise<void> {
  await withFixture(({ controller, destination, source }) => {
    const tab = createTab(false);
    source.append(tab);

    controller.migratePinnedTabs(destination, [tab], source);

    assertIgnored(tab, source, "unpinned source child");
  });
}

async function testIgnoresClosingPinnedSourceChild(): Promise<void> {
  await withFixture(({ controller, destination, source }) => {
    const tab = createTab(true);
    tab.closing = true;
    source.append(tab);

    controller.migratePinnedTabs(destination, [tab], source);

    assertIgnored(tab, source, "closing pinned source child");
  });
}

async function testIgnoresNonElementCandidate(): Promise<void> {
  await withFixture(({ controller, destination, source }) => {
    const text = document!.createTextNode("not a tab");
    source.append(text);

    controller.migratePinnedTabs(destination, [text], source);

    assertEquals(
      text.parentNode,
      source,
      "non-Element node should remain in source",
    );
  });
}

async function testMutationObserverDoesNotResurrectRemovedTab(): Promise<void> {
  await withFixture(async ({ controller, destination, source }) => {
    let deliveredRecords = 0;
    const observer = new MutationObserver((records) => {
      deliveredRecords += records.length;
      for (const record of records) {
        controller.migratePinnedTabs(destination, record.addedNodes, source);
      }
    });
    observer.observe(source, { childList: true });

    try {
      const tab = createTab(true);
      source.append(tab);
      tab.closing = true;
      tab.remove();

      await new Promise<void>((resolve) => setTimeout(resolve, 0));

      assert(
        deliveredRecords > 0,
        "MutationObserver should deliver stale records",
      );
      assertEquals(
        tab.isConnected,
        false,
        "removed tab should stay disconnected after observer delivery",
      );
      assertIgnored(tab, null, "removed tab after observer delivery");
    } finally {
      observer.disconnect();
    }
  });
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "migrates live pinned direct child",
      fn: testMigratesLivePinnedDirectChild,
    },
    {
      name: "ignores connected wrong-parent tab",
      fn: testIgnoresConnectedWrongParent,
    },
    {
      name: "ignores disconnected pinned tab",
      fn: testIgnoresDisconnectedPinnedTab,
    },
    {
      name: "ignores unpinned source child",
      fn: testIgnoresUnpinnedSourceChild,
    },
    {
      name: "ignores closing pinned source child",
      fn: testIgnoresClosingPinnedSourceChild,
    },
    {
      name: "ignores non-Element candidate",
      fn: testIgnoresNonElementCandidate,
    },
    {
      name: "MutationObserver does not resurrect removed tab",
      fn: testMutationObserverDoesNotResurrectRemovedTab,
    },
  ];

  await runTests("pinned-tab-controller.test.ts", tests);
}
