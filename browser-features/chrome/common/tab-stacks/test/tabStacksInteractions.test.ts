// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "solid-js";
import TabStacks, { ENABLED_PREF } from "../index.ts";
import {
  getGBrowser,
  getTabDragId,
  PROXY_DRAG_TYPE,
  STACK_ATTR,
  type StackTab,
  syncActiveGroup,
  TAB_DROP_TYPE,
} from "../stack-bar.tsx";
import { config, setConfig } from "../../designs/configs.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  NativeStackBrowser,
  NativeStackTab,
  NativeStackTransfer,
} from "./types.ts";

const tick = () =>
  new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
async function waitFor(check: () => boolean, message: string): Promise<void> {
  for (let i = 0; i < 100 && !check(); i++) {
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  assert(check(), message);
}

async function testNativeInteractions(): Promise<void> {
  const gb = getGBrowser() as NativeStackBrowser;
  const originalTab = gb.selectedTab;
  const originalConfig = structuredClone(config());
  const prefNames = [ENABLED_PREF, "sidebar.verticalTabs", "sidebar.revamp"];
  const prefs = prefNames.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));
  const created: StackTab[] = [];
  let dispose: (() => void) | undefined;
  let otherWindow: Window | undefined;
  try {
    setConfig((prev) => ({
      ...prev,
      tabbar: { ...prev.tabbar, tabbarStyle: "horizontal" },
    }));
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    Services.prefs.setBoolPref("sidebar.revamp", false);
    Services.prefs.setBoolPref(ENABLED_PREF, true);
    await waitFor(
      () => !gb.tabContainer.verticalMode,
      "Horizontal tabs are ready",
    );
    const feature = Object.create(TabStacks.prototype) as TabStacks;
    feature.logger = console.createInstance({
      prefix: "tab-stacks-interactions-test",
    });
    createRoot((cleanup) => {
      dispose = cleanup;
      feature.init();
    });
    const addTab = () => {
      const tab = gb.addTab("about:blank", {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        inBackground: true,
      }) as NativeStackTab;
      created.push(tab);
      return tab;
    };
    const tabs = Array.from({ length: 36 }, addTab);
    const group = gb.addTabGroup(tabs, { label: "Stack interactions A" });
    const otherTabs = Array.from({ length: 36 }, addTab);
    const otherGroup = gb.addTabGroup(otherTabs, {
      label: "Stack interactions B",
    });
    gb.selectedTab = tabs[0];
    const scroller = () => document.getElementById("floorp-stack-scroller")!;
    const proxy = (tab: StackTab) =>
      document.querySelector<HTMLElement>(
        `.floorp-stack-tab[data-floorp-drag-id="${getTabDragId(tab)}"]`,
      )!;
    await waitFor(() => !!proxy(tabs[0]), "Native tabs have stack proxies");
    await tick();
    assert(
      scroller().scrollWidth > scroller().clientWidth + 500,
      "Stack overflows",
    );
    scroller().scrollLeft = 320;
    gb.selectedTab = originalTab;
    await tick();
    assert(
      !document.getElementById("floorp-stack-scroller"),
      "Normal tab hides the stack row",
    );
    gb.selectedTab = tabs[0];
    await tick();
    assertEquals(
      scroller().scrollLeft,
      320,
      "Returning from a normal tab retains the scroll position",
    );
    gb.selectedTab = otherTabs[0];
    await tick();
    assertEquals(
      scroller().scrollLeft,
      0,
      "A different stack starts at its own position",
    );
    scroller().scrollLeft = 160;
    gb.selectedTab = tabs[0];
    await tick();
    assertEquals(scroller().scrollLeft, 320, "Direct stack switch restores A");
    gb.selectedTab = otherTabs[0];
    await tick();
    assertEquals(scroller().scrollLeft, 160, "Direct stack switch restores B");
    // Workspaces hide groups with display:none and switch selected tabs.
    otherGroup.style.display = "none";
    gb.selectedTab = tabs[0];
    syncActiveGroup();
    await tick();
    otherGroup.style.display = "";
    gb.selectedTab = otherTabs[0];
    await tick();
    assertEquals(
      scroller().scrollLeft,
      160,
      "Workspace-hidden stack retains its position",
    );
    gb.selectedTab = tabs[0];
    await tick();

    const wheel = (deltaMode: number, deltaY: number, deltaX = 0) => {
      scroller().scrollLeft = 0;
      const event = new WheelEvent("wheel", {
        bubbles: true,
        cancelable: true,
        deltaMode,
        deltaY,
        deltaX,
      });
      scroller().dispatchEvent(event);
      assert(event.defaultPrevented, "Overflow wheel is consumed");
      return scroller().scrollLeft;
    };
    assertEquals(
      wheel(WheelEvent.DOM_DELTA_PIXEL, 24),
      24,
      "Pixel wheel retains trackpad precision",
    );
    assertEquals(
      wheel(WheelEvent.DOM_DELTA_PIXEL, 1, 32),
      32,
      "Horizontal dominant axis is used",
    );
    const line = document.getElementById("floorp-stack-items")!.scrollWidth /
      tabs.length;
    assert(
      Math.abs(wheel(WheelEvent.DOM_DELTA_LINE, 1) - line) <= 1,
      "One wheel line scrolls one average tab width",
    );
    assertEquals(
      wheel(WheelEvent.DOM_DELTA_PAGE, 1),
      scroller().clientWidth,
      "Page wheel scrolls one viewport",
    );
    assert(
      wheel(WheelEvent.DOM_DELTA_LINE, 100) <= scroller().clientWidth,
      "Large line wheel is capped to a page",
    );
    scroller().style.direction = "rtl";
    assertEquals(
      wheel(WheelEvent.DOM_DELTA_PIXEL, 24),
      -24,
      "Vertical wheel follows RTL direction",
    );
    scroller().style.direction = "";

    const background = tabs[3];
    proxy(background).dispatchEvent(
      new MouseEvent("click", { button: 2, bubbles: true }),
    );
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "Right click does not select the proxy",
    );
    proxy(background).dispatchEvent(
      new MouseEvent("click", { button: 1, bubbles: true }),
    );
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "Middle click does not select a background tab",
    );
    proxy(background).dispatchEvent(
      new MouseEvent("auxclick", {
        button: 1,
        bubbles: true,
        cancelable: true,
      }),
    );
    await waitFor(
      () => !background.isConnected,
      "Middle click closes the individual native tab",
    );
    assertEquals(
      gb.selectedTab,
      tabs[0],
      "Closing the background proxy keeps selection",
    );
    assertEquals(
      group.tabs.length,
      35,
      "Other stack members survive middle click",
    );

    // Synthetic drag events exercise the real native tab transfer/adoption
    // handlers; OS pointer/desktop drag gestures still need manual coverage.
    const drag = (tab: StackTab) => {
      const source = proxy(tab);
      const dt = new DataTransfer();
      source.dispatchEvent(
        new DragEvent("dragstart", { bubbles: true, dataTransfer: dt }),
      );
      assertEquals(
        dt.types[0],
        TAB_DROP_TYPE,
        "Native tab data is first, before the private proxy flavor",
      );
      assert(
        dt.types.includes(PROXY_DRAG_TYPE),
        "Proxy reordering flavor is retained",
      );
      dt.dropEffect = "move";
      return { source, dt };
    };
    const drop = (target: Element, dt: DataTransfer, after = false) => {
      const rect = target.getBoundingClientRect();
      target.dispatchEvent(
        new DragEvent("drop", {
          bubbles: true,
          cancelable: true,
          dataTransfer: dt,
          clientX: after ? rect.right - 1 : rect.left + 1,
          clientY: rect.top + 5,
        }),
      );
    };
    const end = async (source: Element, dt: DataTransfer) => {
      source.dispatchEvent(
        new DragEvent("dragend", { bubbles: true, dataTransfer: dt }),
      );
      await new Promise((resolve) => setTimeout(resolve, 180));
    };
    scroller().scrollLeft = 0;
    const nativeTabs = [addTab(), addTab()];
    gb.selectedTab = nativeTabs[0] as NativeStackTab;
    gb.addToMultiSelectedTabs(nativeTabs[0]);
    gb.addToMultiSelectedTabs(nativeTabs[1]);
    // A constructed DataTransfer cannot append Gecko's second native item.
    // Exercise the stack's receiving handler with the real first tab and
    // native selectedTabs instead; OS multi-item drag creation is not mocked.
    const nativeTransfer = new DataTransfer() as NativeStackTransfer;
    nativeTransfer.mozSetDataAt(TAB_DROP_TYPE, nativeTabs[0], 0);
    assert(
      nativeTabs.every((tab) => tab.multiselected),
      "Native strip drop starts with both selected tabs",
    );
    nativeTransfer.dropEffect = "move";
    drop(
      otherGroup.querySelector(".tab-group-label-container")!,
      nativeTransfer,
    );
    await waitFor(
      () => nativeTabs.every((tab) => tab.group === otherGroup),
      "Native strip multiselection joins the stack together",
    );
    gb.unlockClearMultiSelection?.();
    gb.clearMultiSelectedTabs?.();
    gb.selectedTab = tabs[0];
    await waitFor(
      () => !!proxy(tabs[0]),
      "Source stack returns after native join",
    );
    gb.addToMultiSelectedTabs(tabs[0]);
    gb.addToMultiSelectedTabs(tabs[2]);
    assert(tabs[2].multiselected, "Proxy starts in a native multiselection");
    const reorder = drag(tabs[2]);
    assert(
      !tabs[2].multiselected && !tabs[0].multiselected,
      "Proxy dragging explicitly chooses a single tab",
    );
    assertEquals(
      (tabs[2]._dragData as { movingTabs: StackTab[] }).movingTabs.length,
      1,
      "Native payload and moving set agree on one proxy tab",
    );
    drop(proxy(tabs[0]), reorder.dt);
    await end(reorder.source, reorder.dt);
    assertEquals(
      group.tabs[0],
      tabs[2],
      "Proxy drop reorders native group members",
    );
    const join = drag(tabs[2]);
    drop(otherGroup.querySelector(".tab-group-label-container")!, join.dt);
    // Deliberately omit dragend: the source proxy is removed on this drop.
    await waitFor(
      () => tabs[2].group === otherGroup,
      "Direct proxy drop joins another stack",
    );
    assertEquals(
      otherGroup.getAttribute(STACK_ATTR),
      "true",
      "Destination remains a stack",
    );
    await waitFor(
      () => !tabs[2]._dragData,
      "Lost dragend is recovered without the removed proxy",
    );
    gb.selectedTab = tabs[0];
    await waitFor(
      () => !!proxy(tabs[0]),
      "Active stack synchronization resumes after lost dragend",
    );
    const eject = drag(tabs[1]);
    drop(gb.tabContainer, eject.dt, true);
    await end(eject.source, eject.dt);
    assertEquals(tabs[1].group, null, "Proxy drop in the strip ejects the tab");

    const openWindow =
      (globalThis as unknown as { OpenBrowserWindow(): Window })
        .OpenBrowserWindow;
    otherWindow = openWindow();
    const foreign = otherWindow as unknown as {
      gBrowser?: NativeStackBrowser;
      gBrowserInit?: { delayedStartupFinished: boolean };
    };
    await waitFor(
      () => !!foreign.gBrowserInit?.delayedStartupFinished,
      "Second browser window starts",
    );
    const destination = foreign.gBrowser!;
    gb.selectedTab = tabs[0];
    await tick();
    const transfer = drag(tabs[4]);
    const previousCount = destination.tabs.length;
    drop(destination.tabContainer, transfer.dt, true);
    await waitFor(
      () =>
        !tabs[4].isConnected && destination.tabs.length === previousCount + 1,
      "Native cross-window drop adopts the proxy tab",
    );
    await waitFor(
      () => !tabs[4]._dragData,
      "Foreign native drop without dragend cleans the source payload",
    );
    gb.selectedTab = otherTabs[0];
    await waitFor(
      () => !!proxy(otherTabs[0]),
      "Foreign drop without dragend releases the source active-stack freeze",
    );
    gb.selectedTab = tabs[0];
    await waitFor(
      () => !!proxy(tabs[0]),
      "Source stack can be reselected after adoption",
    );
    assert(
      tabs[0].isConnected && tabs[0].group === group,
      "Source stack and other tabs survive adoption",
    );
    const destinationGroup = destination.addTabGroup([...destination.tabs], {
      label: "Cross-window stack destination",
    });
    await waitFor(
      () => destinationGroup.hasAttribute(STACK_ATTR),
      "Second window presents its stack",
    );
    gb.addToMultiSelectedTabs(tabs[6]);
    gb.addToMultiSelectedTabs(tabs[8]);
    const chipTransfer = drag(tabs[6]);
    drop(
      destinationGroup.querySelector(".tab-group-label-container")!,
      chipTransfer.dt,
    );
    await waitFor(
      () =>
        !tabs[6].isConnected &&
        destinationGroup.tabs.length === previousCount + 2,
      "Proxy joins another window's stack directly",
    );
    assert(
      tabs[8].isConnected && tabs[8].group === group,
      "Other previously selected tabs stay in their source stack",
    );
    await waitFor(
      () => !tabs[6]._dragData,
      "Cross-window chip join recovers without source dragend",
    );
    const barTransfer = drag(tabs[7]);
    const destinationScroller = otherWindow.document.getElementById(
      "floorp-stack-scroller",
    )!;
    assert(destinationScroller, "Destination stack bar is visible");
    drop(destinationScroller, barTransfer.dt);
    await end(barTransfer.source, barTransfer.dt);
    await waitFor(
      () =>
        !tabs[7].isConnected &&
        destinationGroup.tabs.length === previousCount + 3,
      "Proxy can join another window through its stack bar",
    );
    otherWindow.close();
    otherWindow = undefined;

    // Finish an unhandled drop outside the toolbox through Firefox's real
    // detach handler, including screen geometry and new-window adoption.
    gb.addToMultiSelectedTabs(tabs[5]);
    gb.addToMultiSelectedTabs(tabs[9]);
    const detach = drag(tabs[5]);
    detach.dt.dropEffect = "none";
    const screenX = window.screenX + 100;
    const screenY = window.screenY + window.outerHeight + 100;
    const event = new DragEvent("dragend", {
      bubbles: true,
      dataTransfer: detach.dt,
      screenX,
      screenY,
    });
    const screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
      Ci.nsIScreenManager,
    );
    Object.defineProperty(event, "screen", {
      value: screenManager.screenForRect(screenX, screenY, 1, 1),
    });
    detach.source.dispatchEvent(event);
    await waitFor(
      () => !tabs[5].isConnected,
      "Native dragend detaches a stack tab into a new window",
    );
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const candidate = windows.getNext() as unknown as Window;
      if (candidate !== window && !candidate.closed) otherWindow = candidate;
    }
    assert(otherWindow, "Detach created a second browser window");
    assert(
      tabs[9].isConnected && tabs[9].group === group,
      "Proxy detach leaves other previously selected tabs in the source stack",
    );
  } finally {
    otherWindow?.close();
    for (const tab of created) {
      if (tab.isConnected && !tab.closing) {
        gb.removeTab(tab, { animate: false });
      }
    }
    if (originalTab.isConnected) gb.selectedTab = originalTab;
    syncActiveGroup();
    dispose?.();
    setConfig(originalConfig);
    for (const pref of prefs) {
      if (pref.user) Services.prefs.setBoolPref(pref.name, pref.value);
      else Services.prefs.clearUserPref(pref.name);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("tabStacksInteractions.test.ts", [{
    name:
      "Native stack scrolling, auxiliary clicks and cross-stack/window proxy drags",
    fn: testNativeInteractions,
  }]);
}
