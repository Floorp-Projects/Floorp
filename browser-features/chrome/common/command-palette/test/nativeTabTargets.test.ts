// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { getTabCommands } from "../tab-provider.ts";
import {
  loadTabs,
  tabSwitcherCommand,
} from "../multiInputCommand/switcher/tab-switcher.ts";
import { reopenInContainerCommand } from "../multiInputCommand/reopen-in-container.ts";

async function until(predicate: () => boolean, message: string): Promise<void> {
  const end = Date.now() + 15000;
  while (Date.now() < end) {
    if (predicate()) return;
    await new Promise((resolve) => globalThis.setTimeout(resolve, 25));
  }
  assert(predicate(), message);
}

async function withTabs(
  action: (tabs: XULElement[], urls: string[]) => Promise<void>,
): Promise<void> {
  const gb = globalThis.gBrowser;
  const before = new Set(gb.tabs);
  const selected = gb.selectedTab;
  const nonce = crypto.randomUUID();
  const urls = [0, 1, 2, 3].map((n) =>
    `about:blank#floorp-native-tab-${nonce}-${n}`
  );
  const tabs = urls.map((url) =>
    gb.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      userContextId: 0,
    })
  );
  try {
    await until(
      () =>
        tabs.every((tab, i) => tab.linkedBrowser?.currentURI?.spec === urls[i]),
      "Actual native fixture tabs finish navigation",
    );
    await action(tabs, urls);
  } finally {
    if (selected.isConnected && gb.tabs.includes(selected)) {
      gb.selectedTab = selected;
    }
    for (const tab of Array.from(gb.tabs)) {
      if (!before.has(tab)) gb.removeTab(tab, { animate: false });
    }
  }
}

async function testDynamicTabCommandsKeepIdentity(): Promise<void> {
  await withTabs(async (tabs, urls) => {
    const gb = globalThis.gBrowser;
    const commands = urls.map((url) =>
      getTabCommands(window).find((command) => command.description === url)
    );
    assert(commands.every(Boolean), "Every native tab has a dynamic command");
    const ids = commands.map((command) => command!.id);
    assertEquals(new Set(ids).size, urls.length, "Native tabs have unique IDs");
    assert(
      ids.every((id) => !id.includes("undefined")),
      "Dynamic IDs do not require obsolete native tab fields",
    );
    gb.moveTabTo(tabs[2], { tabIndex: Array.from(gb.tabs).indexOf(tabs[0]) });
    for (let i = 0; i < urls.length; i++) {
      assertEquals(
        getTabCommands(window).find((command) =>
          command.description === urls[i]
        )?.id,
        ids[i],
        "Reordering preserves the command's tab identity",
      );
    }
    commands[2]!.fn(window);
    assertEquals(
      gb.selectedTab,
      tabs[2],
      "Captured command selects its moved tab",
    );
    gb.selectedTab = tabs[0];
    gb.removeTab(tabs[2]);
    commands[2]!.fn(window);
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "A closing command cannot select a removed target",
    );
    await until(() => !tabs[2].isConnected, "Target tab closes");
    commands[2]!.fn(window);
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "A closed command cannot select a replacement tab",
    );
  });
}

async function testTabChoicesSurviveReorderAndRejectClosedTargets(): Promise<
  void
> {
  await withTabs(async (tabs, urls) => {
    const gb = globalThis.gBrowser;
    gb.selectedTab = tabs[0];
    const loaded = await loadTabs(window);
    assert(!Array.isArray(loaded), "Native choices expose a selected default");
    assert(
      typeof loaded.defaultIndex === "number",
      "Native choices provide a default index",
    );
    const choice = loaded.choices.find((item) =>
      item.description?.startsWith(urls[2])
    );
    assert(choice, "Target native tab has a step choice");
    assert(
      loaded.choices[loaded.defaultIndex]?.description?.startsWith(urls[0]),
      "Default choice follows the actual selected tab",
    );
    gb.moveTabTo(tabs[2], { tabIndex: Array.from(gb.tabs).indexOf(tabs[0]) });
    tabSwitcherCommand.fn(window, { tab: choice.value });
    assertEquals(
      gb.selectedTab,
      tabs[2],
      "Choice selects its original tab after reorder",
    );
    gb.selectedTab = tabs[0];
    gb.removeTab(tabs[2]);
    tabSwitcherCommand.fn(window, { tab: choice.value });
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "A closing choice cannot retarget selection",
    );
    await until(() => !tabs[2].isConnected, "Choice target tab closes");
    tabSwitcherCommand.fn(window, { tab: choice.value });
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "A stale choice never selects the next tab at its old position",
    );
    tabSwitcherCommand.fn(window, {
      tab: String(Array.from(gb.tabs).indexOf(tabs[1])),
    });
    assertEquals(
      gb.selectedTab,
      tabs[1],
      "Numeric shortcut arguments retain flat zero-based position semantics",
    );
    tabSwitcherCommand.fn(window, { tab: "undefined" });
    assertEquals(
      gb.selectedTab,
      tabs[1],
      "Invalid choice leaves selection unchanged",
    );
  });
}

async function testTabTargetsUseTheSuppliedNativeWindow(): Promise<void> {
  const open = window as unknown as {
    OpenBrowserWindow(): Window & {
      gBrowser: GBrowser;
      gBrowserInit?: { delayedStartupFinished?: boolean };
    };
  };
  const other = open.OpenBrowserWindow();
  const originalSelected = globalThis.gBrowser.selectedTab;
  try {
    await until(
      () => !!other.gBrowserInit?.delayedStartupFinished && !!other.gBrowser,
      "Second native browser window finishes startup",
    );
    const gb = other.gBrowser;
    const nonce = crypto.randomUUID();
    const urls = [0, 1].map((i) =>
      `about:blank#floorp-target-window-${nonce}-${i}`
    );
    const tabs = urls.map((url) =>
      gb.addTab(url, {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      })
    );
    await until(
      () =>
        tabs.every((tab, i) => tab.linkedBrowser?.currentURI?.spec === urls[i]),
      "Second-window native tabs finish navigation",
    );
    gb.selectedTab = tabs[0];
    const loaded = await loadTabs(other);
    const choices = Array.isArray(loaded) ? loaded : loaded.choices;
    const choice = choices.find((item) =>
      item.description?.startsWith(urls[1])
    );
    assert(choice, "Choice loader reads the supplied native window");
    tabSwitcherCommand.fn(other, { tab: choice.value });
    assertEquals(
      gb.selectedTab,
      tabs[1],
      "Switch Tab selects within the supplied window",
    );
    assertEquals(
      globalThis.gBrowser.selectedTab,
      originalSelected,
      "Switch Tab leaves the module's window unchanged",
    );
    const command = getTabCommands(other).find((item) =>
      item.description === urls[1]
    );
    assert(command, "Second-window dynamic command exists");
    gb.selectedTab = tabs[0];
    command.fn(window);
    assertEquals(
      globalThis.gBrowser.selectedTab,
      originalSelected,
      "A foreign-window command cannot select its captured tab here",
    );
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "A rejected foreign target leaves its owner unchanged",
    );
    command.fn(other);
    assertEquals(
      gb.selectedTab,
      tabs[1],
      "Dynamic command selects in its live owning window",
    );
  } finally {
    other.close();
    await until(() => other.closed, "Second native test window closes");
  }
}

async function testContainerReopenPreservesNativePosition(): Promise<void> {
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  ContextualIdentityService.ensureDataReady();
  const identity = ContextualIdentityService.create(
    "Floorp native tab regression",
    "fingerprint",
    "blue",
  );
  try {
    await withTabs(async (tabs, urls) => {
      const gb = globalThis.gBrowser;
      const target = tabs[2];
      gb.selectedTab = target;
      const before = Array.from(gb.tabs);
      const position = before.indexOf(target);
      reopenInContainerCommand.fn(window, {
        container: String(identity.userContextId),
      });
      await until(
        () => !gb.tabs.includes(target),
        "Container reopen removes its source tab",
      );
      const created = Array.from(gb.tabs).filter((tab) =>
        !before.includes(tab)
      );
      assertEquals(
        created.length,
        1,
        "Container reopen creates exactly one replacement",
      );
      const replacement = created[0];
      assertEquals(
        Array.from(gb.tabs).indexOf(replacement),
        position,
        "Container replacement retains its native flat tab position",
      );
      await until(
        () => replacement.linkedBrowser?.currentURI?.spec === urls[2],
        "Replacement loads the source URL",
      );
      assertEquals(
        Number(replacement.getAttribute("usercontextid")),
        identity.userContextId,
        "Replacement uses the requested container",
      );
      assertEquals(
        gb.selectedTab,
        replacement,
        "Selected source transfers selection to the replacement",
      );
      assertEquals(
        gb.tabs[position - 1],
        before[position - 1],
        "Left neighbor stays adjacent",
      );
      assertEquals(
        gb.tabs[position + 1],
        before[position + 1],
        "Right neighbor stays adjacent",
      );
    });
  } finally {
    ContextualIdentityService.remove(identity.userContextId);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("nativeTabTargets.test.ts", [
    {
      name: "dynamic native tab commands preserve unique live identities",
      fn: testDynamicTabCommandsKeepIdentity,
    },
    {
      name:
        "native Switch Tab choices survive reorder and reject closed targets",
      fn: testTabChoicesSurviveReorderAndRejectClosedTargets,
    },
    {
      name:
        "native tab choice and dynamic commands honor their supplied window",
      fn: testTabTargetsUseTheSuppliedNativeWindow,
    },
    {
      name: "native container reopen preserves source position and neighbors",
      fn: testContainerReopenPreservesNativePosition,
    },
  ]);
}
