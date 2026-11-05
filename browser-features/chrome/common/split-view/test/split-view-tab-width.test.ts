// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import splitViewCss from "../styles/split-view.css?raw";
import { config, setConfig } from "../../designs/configs.ts";
import { assert, runTests } from "../../../test/utils/test_harness.ts";

type TestTab = XULElement & { pinned: boolean };
type TestBrowser = typeof gBrowser & {
  addTabGroup(tabs: TestTab[], options: { label: string }): XULElement;
  addTabSplitView(
    tabs: TestTab[],
    options: { insertBefore: TestTab },
  ): XULElement;
};

async function waitFor(check: () => boolean, message: string): Promise<void> {
  for (let i = 0; i < 150; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 20));
  }
  assert(check(), message);
}

async function testHorizontalTabsWithSplit(): Promise<void> {
  const browser = gBrowser as TestBrowser;
  const originalConfig = structuredClone(config());
  const originalTab = browser.selectedTab;
  const prefNames = [
    "sidebar.verticalTabs",
    "sidebar.revamp",
    "browser.tabs.splitView.enabled",
    "browser.tabs.splitview.hasUsed",
  ];
  const prefs = prefNames.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));
  const tabs: TestTab[] = [];
  const style = document.createElement("style");
  // The exact source under test is loaded after the running browser's styles.
  style.textContent = splitViewCss;
  try {
    Services.prefs.setBoolPref("browser.tabs.splitView.enabled", true);
    Services.prefs.setBoolPref("sidebar.revamp", false);
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    setConfig((previous) => ({
      ...previous,
      tabbar: { ...previous.tabbar, tabbarStyle: "horizontal" },
      tab: { ...previous.tab, tabMinWidth: 76 },
    }));
    await waitFor(
      () => !browser.tabContainer.verticalMode,
      "horizontal tab bar",
    );
    await waitFor(
      () =>
        getComputedStyle(document.documentElement).getPropertyValue(
          "--floorp-tab-min-width",
        ).trim() === "76px",
      "76px configured tab minimum",
    );
    document.head!.append(style);
    for (let i = 0; i < 40; i++) {
      tabs.push(browser.addTab("about:blank", {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        inBackground: true,
        skipAnimation: true,
      }) as TestTab);
    }
    const group = browser.addTabGroup([tabs[14], tabs[15]], {
      label: "width regression",
    });
    browser.addTabSplitView([tabs[10], tabs[11]], { insertBefore: tabs[10] });
    browser.selectedTab = tabs[10];
    const toolbar = document.getElementById("TabsToolbar")!;
    await waitFor(
      () =>
        toolbar.hasAttribute("splitview-multibar") &&
        tabs[10].hasAttribute("data-floorp-split-tab"),
      "split toolbar and native pane markers",
    );
    const strip = browser.tabContainer.arrowScrollbox.scrollbox;
    await waitFor(
      () => strip.scrollWidth > strip.clientWidth,
      "tabs overflow the viewport",
    );
    // Gecko animates split-tab insertion; measure its settled geometry.
    await new Promise((resolve) => setTimeout(resolve, 1000));
    const splitWidths = [tabs[10], tabs[11]].map((tab) =>
      tab.getBoundingClientRect().width
    );
    assert(
      splitWidths.every((width) => width > 0),
      "split panes are visible in the strip",
    );
    for (const width of [76, 200]) {
      setConfig((previous) => ({
        ...previous,
        tab: { ...previous.tab, tabMinWidth: width },
      }));
      await waitFor(
        () =>
          getComputedStyle(document.documentElement).getPropertyValue(
            "--floorp-tab-min-width",
          ).trim() === `${width}px`,
        `configured ${width}px minimum`,
      );
      await waitFor(
        () => Math.abs(tabs[8].getBoundingClientRect().width - width) < 1,
        `ordinary tab retains configured ${width}px minimum`,
      );
      assert(
        Math.abs(tabs[14].getBoundingClientRect().width - width) < 1,
        "ordinary group tab retains configured width",
      );
      assert(
        group.contains(tabs[14]) && group.contains(tabs[15]),
        "ordinary group remains intact",
      );
      for (let i = 0; i < 2; i++) {
        assert(
          Math.abs(
            tabs[10 + i].getBoundingClientRect().width - splitWidths[i],
          ) <= 3,
          `split tab width ${tabs[10 + i].getBoundingClientRect().width} vs ${
            splitWidths[i]
          } at ${width}px`,
        );
      }
      const left = tabs[9].getBoundingClientRect();
      const wrapper = tabs[10].parentElement!.getBoundingClientRect();
      const right = tabs[12].getBoundingClientRect();
      assert(
        left.right <= wrapper.left + 1 && wrapper.right <= right.left + 1,
        "normal tabs do not overlap the split wrapper",
      );
      assert(
        strip.scrollWidth > strip.clientWidth,
        "tab strip stays scrollable",
      );
      strip.scrollLeft = 0;
      strip.scrollLeft = 150;
      await waitFor(
        () => strip.scrollLeft > 0,
        "overflow scroll position advances",
      );
    }
  } finally {
    if (originalTab?.isConnected) browser.selectedTab = originalTab;
    for (const tab of tabs.reverse()) {
      if (tab.isConnected) browser.removeTab(tab, { animate: false });
    }
    style.remove();
    setConfig(originalConfig);
    for (const pref of prefs) {
      if (pref.user) Services.prefs.setBoolPref(pref.name, pref.value);
      else Services.prefs.clearUserPref(pref.name);
    }
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("split-view-tab-width.test.ts", [{
    name: "configured width, split layout, group and overflow remain usable",
    fn: testHorizontalTabsWithSplit,
  }]);
}
