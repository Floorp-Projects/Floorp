// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { ENABLED_PREF, GROUP_KINDS_PREF, setGroupKind } from "../index.ts";
import { STACK_ATTR } from "../stack-bar.tsx";
import type { StackGroup } from "../stack-bar.tsx";
import type {
  SessionStackWindow,
  StackSessionStore,
  StackSessionWindowState,
} from "./startup-test-types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const mainWindow = window as unknown as SessionStackWindow;
const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: StackSessionStore };

async function waitFor(check: () => boolean, label: string): Promise<void> {
  for (let i = 0; i < 400; i++) {
    if (check()) return;
    await new Promise((resolve) => mainWindow.setTimeout(resolve, 25));
  }
  throw new Error(`Timed out waiting for Tab Stacks session restore: ${label}`);
}

function group(win: SessionStackWindow, id: string): StackGroup | undefined {
  return win.gBrowser.tabGroups.find((item) => item.id === id);
}

function windowState(win: SessionStackWindow): StackSessionWindowState {
  const state = SessionStore.getWindowState(win);
  return (typeof state === "string" ? JSON.parse(state) : state).windows[0];
}

async function waitForLoadedWindow(win: SessionStackWindow): Promise<void> {
  await waitFor(
    () => !!win.gBrowserInit?.delayedStartupFinished,
    "native delayed startup",
  );
  // The normal per-window module loader must install this feature. Importing
  // TabStacks into the test window and hand-initializing it for a foreign
  // window would miss both the loader ordering and window ownership bugs.
  await waitFor(
    () => !!win.document.getElementById("floorp-stack-kind-menu"),
    "loader-owned Tab Stacks initialization",
  );
}

async function closeWindow(win: SessionStackWindow): Promise<void> {
  if (win.closed) return;
  let unloaded = false;
  const onUnload = () => unloaded = true;
  win.addEventListener("unload", onUnload, { once: true });
  try {
    win.close();
    await waitFor(() => win.closed && unloaded, "window unload");
  } finally {
    win.removeEventListener("unload", onUnload);
  }
}

async function testNativeTwoWindowRestorePreservesStacksAndGroups(): Promise<
  void
> {
  const prefNames = [
    ENABLED_PREF,
    "sidebar.verticalTabs",
    "browser.warnOnQuit",
    "browser.tabs.warnOnClose",
  ];
  const savedBoolPrefs = prefNames.map((name) => ({
    name,
    hadUser: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));
  const hadKinds = Services.prefs.prefHasUserValue(GROUP_KINDS_PREF);
  const kinds = Services.prefs.getStringPref(GROUP_KINDS_PREF, "{}");
  const windows: SessionStackWindow[] = [];
  const records: Array<
    { stackId: string; groupId: string; stackSize: number }
  > = [];
  try {
    Services.prefs.setBoolPref(ENABLED_PREF, true);
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    Services.prefs.setBoolPref("browser.warnOnQuit", false);
    Services.prefs.setBoolPref("browser.tabs.warnOnClose", false);
    // These are real normal starts, with no cross-window feature constructor.
    for (let i = 0; i < 2; i++) {
      const win = mainWindow.OpenBrowserWindow();
      windows.push(win);
      await waitForLoadedWindow(win);
      const gb = win.gBrowser;
      const tabs = Array.from({ length: 4 }, (_, tab) =>
        gb.addTab(
          `about:robots#tab-stacks-${i}-${tab}`,
          {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
            inBackground: true,
          },
        ));
      const stack = gb.addTabGroup(tabs.slice(0, 2), {
        label: `Restore stack ${i}`,
      });
      const native = gb.addTabGroup(tabs.slice(2), {
        label: `Restore group ${i}`,
      });
      records.push({
        stackId: stack.id,
        groupId: native.id,
        stackSize: stack.tabs.length,
      });
      setGroupKind(native.id, "group");
      // Group kinds are refreshed by native events, as in actual selection.
      gb.selectedTab = tabs[0];
      win.dispatchEvent(new Event("TabSelect"));
      await waitFor(
        () =>
          stack.hasAttribute(STACK_ATTR) && !native.hasAttribute(STACK_ATTR) &&
          win.document.querySelectorAll("#floorp-stack-items .floorp-stack-tab")
              .length === 2,
        `normal window ${i} has a stack bar and a native group`,
      );
      assertEquals(
        win.document.querySelectorAll("#floorp-stack-bar").length,
        1,
        "normal startup installs one second row",
      );
      const state = windowState(win);
      assert(
        state.groups?.some((item) => item.id === stack.id),
        "native SessionStore saves stack group identity",
      );
      assert(
        state.groups?.some((item) => item.id === native.id),
        "native SessionStore saves plain group identity",
      );
      await waitFor(
        () =>
          windowState(win).tabs.some((tab) =>
            tab.entries?.some((entry) =>
              entry.url?.includes(`tab-stacks-${i}-`)
            )
          ),
        "test content has restorable native history",
      );
    }

    // Closing and undoing a window goes through native SessionStore startup
    // restoration. It is an in-process regression test, not a process crash.
    for (const win of windows.slice()) await closeWindow(win);
    for (const record of records) {
      await waitFor(
        () =>
          SessionStore.getClosedWindowData().some((item) =>
            item.groups?.some((entry) => entry.id === record.stackId)
          ),
        "closed native window state is captured",
      );
    }
    const restored: SessionStackWindow[] = [];
    for (const record of records.slice().reverse()) {
      const index = SessionStore.getClosedWindowData().findIndex((item) =>
        item.groups?.some((entry) => entry.id === record.stackId)
      );
      assert(
        index >= 0,
        "the exact test window is in native closed-window data",
      );
      const win = SessionStore.undoCloseWindow(index);
      windows.push(win);
      restored.push(win);
      // Do not await between dispatching the two restores: each window must
      // handle startup and SSWindowRestored in its own natural order.
    }
    for (const win of restored) {
      await waitForLoadedWindow(win);
      const record = records.find((item) => !!group(win, item.stackId));
      assert(record, "restored window has its original group identities");
      await waitFor(
        () =>
          !!group(win, record.stackId)?.hasAttribute(STACK_ATTR) &&
          !group(win, record.groupId)?.hasAttribute(STACK_ATTR),
        "restoration respects saved stack and native group choices",
      );
      const stack = group(win, record.stackId)!;
      assertEquals(
        stack.tabs.length,
        record.stackSize,
        "all original stack members are restored",
      );
      win.gBrowser.selectedTab = stack.tabs[0];
      await waitFor(
        () =>
          win.document.querySelectorAll("#floorp-stack-items .floorp-stack-tab")
            .length === record.stackSize,
        "restored selected stack has the complete second row",
      );
      assertEquals(
        win.document.querySelectorAll("#floorp-stack-bar").length,
        1,
        "restored window installs no duplicate second row",
      );
      assertEquals(
        win.document.querySelectorAll("#floorp-stack-kind-menu").length,
        1,
        "restored window installs no duplicate menu",
      );
    }
    const survivor = restored[1];
    const bar = survivor.document.getElementById("floorp-stack-bar");
    const survivorRecord = records.find((item) =>
      !!group(survivor, item.stackId)
    )!;
    await closeWindow(restored[0]);
    assert(
      !survivor.closed,
      "closing one restored window leaves its sibling alive",
    );
    assertEquals(
      survivor.document.getElementById("floorp-stack-bar"),
      bar,
      "closing one restored window preserves its sibling's bar",
    );
    const stack = group(survivor, survivorRecord.stackId)!;
    survivor.gBrowser.selectedTab = stack.tabs[1];
    await waitFor(
      () =>
        survivor.document.querySelectorAll(
          '#floorp-stack-items .floorp-stack-tab[data-selected="true"]',
        ).length === 1,
      "surviving restored window remains reactive",
    );
  } finally {
    try {
      const results = await Promise.allSettled(
        windows.filter((win) => !win.closed).map(closeWindow),
      );
      // Remove only these test-created windows from the native undo list so
      // later chrome tests do not inherit their group IDs or history.
      const closed = SessionStore.getClosedWindowData();
      for (let i = closed.length - 1; i >= 0; i--) {
        if (
          closed[i].groups?.some((item) =>
            records.some((record) => record.stackId === item.id)
          )
        ) {
          SessionStore.forgetClosedWindow(i);
        }
      }
      for (const result of results) {
        assert(
          result.status === "fulfilled",
          `test window cleanup completes: ${
            result.status === "rejected" ? String(result.reason) : "closed"
          }`,
        );
      }
    } finally {
      if (hadKinds) Services.prefs.setStringPref(GROUP_KINDS_PREF, kinds);
      else Services.prefs.clearUserPref(GROUP_KINDS_PREF);
      for (const pref of savedBoolPrefs) {
        if (pref.hadUser) Services.prefs.setBoolPref(pref.name, pref.value);
        else Services.prefs.clearUserPref(pref.name);
      }
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksSessionRestore.test.ts", [
    {
      name:
        "native SessionStore restores two loader-owned windows with stack choices and independent lifetimes",
      fn: testNativeTwoWindowRestorePreservesStacksAndGroups,
    },
  ]);
}
