// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "solid-js";
import TabStacks, { ENABLED_PREF, GROUP_KINDS_PREF } from "../index.ts";
import { getGBrowser, STACK_ATTR, syncActiveGroup } from "../stack-bar.tsx";
import { config, setConfig } from "../../designs/configs.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  NativeStackBrowser,
  NativeStackTab,
  StackTestPopup,
  StackTestSidebar,
} from "./types.ts";

async function waitFor(check: () => boolean, message: string): Promise<void> {
  for (let i = 0; i < 100; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  assert(check(), message);
}

/** Use native groups and split wrappers: flat mock tabs cannot catch this regression. */
async function testNativeStackSplitLifecycle(): Promise<void> {
  const gb = getGBrowser() as NativeStackBrowser;
  assert(gb, "Native browser is available");
  const sidebar =
    (globalThis as unknown as { SidebarController: StackTestSidebar })
      .SidebarController;
  const originalSidebar = sidebar.getUIState();
  const originalConfig = structuredClone(config());
  const originalTab = gb.selectedTab;
  const prefNames = [
    ENABLED_PREF,
    GROUP_KINDS_PREF,
    "sidebar.verticalTabs",
    "sidebar.revamp",
    "browser.tabs.splitView.enabled",
    "browser.tabs.splitview.hasUsed",
  ];
  const savedPrefs = prefNames.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    bool: Services.prefs.getPrefType(name) === 128,
    value: Services.prefs.getPrefType(name) === 128
      ? Services.prefs.getBoolPref(name)
      : Services.prefs.getStringPref(name, ""),
  }));
  const created: NativeStackTab[] = [];
  let dispose: (() => void) | undefined;
  try {
    Services.prefs.setBoolPref(ENABLED_PREF, true);
    Services.prefs.setBoolPref("browser.tabs.splitView.enabled", true);
    // Own the init subscriptions/render roots so this test can dispose them.
    // Avoid the constructor's module-wide HMR root, which outlives the test.
    const feature = Object.create(TabStacks.prototype) as TabStacks;
    feature.logger = console.createInstance({
      prefix: "tab-stacks-native-test",
    });
    createRoot((cleanup) => {
      dispose = cleanup;
      feature.init();
    });

    setConfig((prev) => ({
      ...prev,
      tabbar: {
        ...prev.tabbar,
        tabbarStyle: "horizontal",
      },
    }));
    Services.prefs.setBoolPref("sidebar.revamp", false);
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    await waitFor(
      () => !gb.tabContainer.verticalMode,
      "Horizontal orientation applied",
    );
    for (const paneCount of [2, 4]) {
      const tabs = Array.from(
        { length: paneCount + 1 },
        () =>
          gb.addTab("about:blank", {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
            inBackground: true,
          }) as NativeStackTab,
      );
      created.push(...tabs);
      const group = gb.addTabGroup(tabs, {
        label: "Native stack regression",
      });
      gb.selectedTab = tabs[0];
      const proxies = () => [
        ...document.querySelectorAll<HTMLElement>(
          "#floorp-stack-items .floorp-stack-tab",
        ),
      ];
      await waitFor(
        () => proxies().length === tabs.length,
        "Stack bar contains every member",
      );
      const chip = group.querySelector(".tab-group-label-container")!;
      const initialHeight = chip.getBoundingClientRect().height;
      assert(
        initialHeight > 0 && initialHeight < 100,
        "Horizontal chip occupies one row",
      );
      const split = gb.addTabSplitView(tabs.slice(0, paneCount), {
        insertBefore: tabs[0],
      });
      await waitFor(
        () => tabs[0].hasAttribute("floorpSplitViewGroupId"),
        "Split session markers applied",
      );
      assertEquals(
        group.getAttribute(STACK_ATTR),
        "true",
        "Creating a split preserves the stack",
      );
      assertEquals(tabs[0].group, group, "Split panes remain in their group");
      assertEquals(
        proxies().length,
        tabs.length,
        "No missing or duplicate pane proxies",
      );
      const rect = split.getBoundingClientRect();
      assert(
        rect.width === 0 && rect.height === 0,
        "Native split wrapper takes no space in the tab strip",
      );
      assert(
        chip.getBoundingClientRect().height < 100,
        "Split does not stretch the chip",
      );

      // Selecting an ordinary member and either pane keeps the bar alive.
      for (const i of [paneCount, 1, 0]) {
        proxies()[i].dispatchEvent(
          new MouseEvent("click", { bubbles: true }),
        );
        assertEquals(
          gb.selectedTab,
          tabs[i],
          "Proxy selects the intended native tab",
        );
        assertEquals(
          proxies().length,
          tabs.length,
          "Selection preserves the bar",
        );
      }
      const panel = document.getElementById(tabs[0].linkedPanel);
      assert(
        panel && panel.getBoundingClientRect().width > 0,
        "Hiding split tabs does not hide their content",
      );

      const savedKinds = Services.prefs.getStringPref(GROUP_KINDS_PREF, "{}");
      for (const expanded of [true, false]) {
        setConfig((prev) => ({
          ...prev,
          tabbar: { ...prev.tabbar, tabbarStyle: "vertical" },
        }));
        Services.prefs.setBoolPref("sidebar.revamp", true);
        Services.prefs.setBoolPref("sidebar.verticalTabs", true);
        await waitFor(
          () =>
            gb.tabContainer.verticalMode && !group.hasAttribute(STACK_ATTR) &&
            proxies().length === 0,
          "Vertical mode suspends stack presentation",
        );
        await sidebar.updateUIState({
          launcherExpanded: expanded,
          launcherVisible: true,
        });
        assert(
          split.getBoundingClientRect().width > 0,
          "Vertical mode shows the native split wrapper",
        );
        assert(
          !document.getElementById("floorp-stack-bar"),
          "Vertical mode has no stack bar in either location",
        );
        assertEquals(
          Services.prefs.getStringPref(GROUP_KINDS_PREF, "{}"),
          savedKinds,
          "Suspension preserves saved group kinds",
        );
        assertEquals(
          tabs[0].group,
          group,
          "Suspension preserves group membership",
        );
        const event = new MouseEvent("contextmenu", {
          bubbles: true,
          cancelable: true,
        });
        chip.dispatchEvent(event);
        const stackMenu = document.getElementById(
          "floorp-stack-kind-menu",
        ) as unknown as StackTestPopup;
        assert(
          stackMenu.state === "closed",
          "Vertical mode does not open the stack conversion menu",
        );
        // Native group handlers may preventDefault and open their own popup.
        for (const popup of document.querySelectorAll("panel, menupopup")) {
          const nativePopup = popup as unknown as StackTestPopup;
          if (nativePopup.state === "open") nativePopup.hidePopup();
        }
        // Groups created while vertical also stay native.
        const extra = gb.addTab("about:blank", {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
          inBackground: true,
        }) as NativeStackTab;
        created.push(extra);
        const other = gb.addTabGroup([extra], {
          label: "Vertical native group",
        });
        await new Promise<void>((resolve) =>
          requestAnimationFrame(() => resolve())
        );
        assert(
          !other.hasAttribute(STACK_ATTR),
          "New vertical groups are native",
        );
        group.collapsed = true;
        assert(
          group.collapsed,
          "Vertical groups retain native collapse behavior",
        );
        group.collapsed = false;
        gb.selectedTab = tabs[0];
        setConfig((prev) => ({
          ...prev,
          tabbar: { ...prev.tabbar, tabbarStyle: "horizontal" },
        }));
        Services.prefs.setBoolPref("sidebar.verticalTabs", false);
        await waitFor(
          () =>
            !gb.tabContainer.verticalMode && group.hasAttribute(STACK_ATTR) &&
            proxies().length === tabs.length,
          "Returning to horizontal restores the stack without restart",
        );
        assertEquals(
          split.tabs.length,
          paneCount,
          "Orientation changes preserve split panes",
        );
        assertEquals(
          other.getAttribute(STACK_ATTR),
          "true",
          "Group created vertically uses its saved default on horizontal return",
        );
        gb.removeTab(extra, { animate: false });
      }

      const toggleKind = async (label: string) => {
        chip.dispatchEvent(
          new MouseEvent("contextmenu", { bubbles: true, cancelable: true }),
        );
        const menu = document.getElementById(
          "floorp-stack-kind-menu",
        ) as unknown as StackTestPopup;
        await waitFor(
          () => menu.state === "open",
          "Stack conversion menu opens even with split panes",
        );
        const item = [...menu.children].find((el) =>
          el.getAttribute("label") === label
        );
        assert(item, `Conversion command exists: ${label}`);
        item.dispatchEvent(new Event("command", { bubbles: true }));
        menu.hidePopup();
        await waitFor(
          () => menu.state === "closed",
          "Conversion menu closes",
        );
      };
      await toggleKind("Change to Tab Group");
      await waitFor(
        () => !group.hasAttribute(STACK_ATTR),
        "Explicit native group choice is respected",
      );
      await waitFor(
        () => split.getBoundingClientRect().width > 0,
        "Native group shows its split wrapper",
      );
      await toggleKind("Change to Tab Stack");
      await waitFor(
        () => proxies().length === tabs.length,
        "Can convert a split-containing group back to a stack",
      );

      split.unsplitTabs();
      await waitFor(() => !tabs[0].splitview, "Split is removed");
      assertEquals(
        group.getAttribute(STACK_ATTR),
        "true",
        "Unsplit preserves the stack",
      );
      assertEquals(
        proxies().length,
        tabs.length,
        "Unsplit preserves every proxy",
      );
      // Closing one of two panes also removes the native wrapper. The
      // remaining pane must stay selected in the stack, with no stale proxy.
      gb.addTabSplitView(tabs.slice(0, 2), { insertBefore: tabs[0] });
      gb.selectedTab = tabs[0];
      // Separate creation and closing as real UI actions; the native
      // wrapper observes its tab membership at the next microtask checkpoint.
      await new Promise<void>((resolve) =>
        requestAnimationFrame(() => resolve())
      );
      proxies()[0].querySelector(".floorp-stack-tab-close")!.dispatchEvent(
        new MouseEvent("click", { bubbles: true }),
      );
      await waitFor(
        () => proxies().length === tabs.length - 1 && !tabs[1].splitview,
        "Closing a pane removes its proxy and dissolves the two-pane split",
      );
      assertEquals(
        gb.selectedTab,
        tabs[1],
        "Close selects the surviving stack member",
      );
      assertEquals(
        group.getAttribute(STACK_ATTR),
        "true",
        "Closing a split pane preserves the stack",
      );
      for (const tab of tabs) {
        if (tab.isConnected) gb.removeTab(tab, { animate: false });
      }
      await waitFor(() => !group.isConnected, "Test group is removed");
    }
  } finally {
    for (const tab of created) {
      if (tab.isConnected && !tab.closing) {
        gb.removeTab(tab, { animate: false });
      }
    }
    if (originalTab.isConnected) gb.selectedTab = originalTab;
    syncActiveGroup();
    dispose?.();
    setConfig(originalConfig);
    for (const pref of savedPrefs) {
      if (!pref.user) Services.prefs.clearUserPref(pref.name);
      else if (pref.bool) {
        Services.prefs.setBoolPref(pref.name, pref.value as boolean);
      } else Services.prefs.setStringPref(pref.name, pref.value as string);
    }
    await sidebar.updateUIState(originalSidebar);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksNative.test.ts", [
    {
      name:
        "Horizontal splits preserve stacks; vertical mode suspends and restores them",
      fn: testNativeStackSplitLifecycle,
    },
  ]);
}
