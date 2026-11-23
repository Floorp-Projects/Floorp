// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import splitViewStyles from "../styles/split-view.css?inline";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  AdoptionBrowser,
  AdoptionTab,
  AdoptionWindow,
} from "./adoption-types.ts";

async function waitUntil(check: () => boolean, message: string): Promise<void> {
  for (let attempt = 0; attempt < 100; attempt++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
  assert(check(), message);
}

/** Check that hidden browser frames neither take flex space nor intercept hits. */
async function assertPaneLayout(
  browser: AdoptionBrowser,
  visible: AdoptionTab[],
  hidden: AdoptionTab[],
): Promise<void> {
  browser.selectedTab = visible[0];
  await new Promise((resolve) => setTimeout(resolve, 300));
  const panels = document.getElementById("tabbrowser-tabpanels")!;
  const bounds = panels.getBoundingClientRect();
  const panelFor = (tab: AdoptionTab): Element => {
    let panel: Element = tab.linkedBrowser;
    while (panel.parentElement && panel.parentElement !== panels) {
      panel = panel.parentElement;
    }
    assert(panel.parentElement === panels, "browser must belong to tabpanels");
    return panel;
  };
  const rects = visible.map((tab) => {
    const panel = panelFor(tab);
    const rect = panel.getBoundingClientRect();
    assertEquals(
      getComputedStyle(panel)!.visibility,
      "visible",
      "active pane is visible",
    );
    assert(
      rect.width >= bounds.width / visible.length - 24,
      "inactive panes must not shrink active panes",
    );
    assert(
      rect.height > bounds.height * 0.9,
      "active pane fills content height",
    );
    const hit = document.elementFromPoint(
      rect.x + rect.width / 2,
      rect.y + rect.height / 2,
    );
    assert(
      hit !== null && panel.contains(hit),
      "active pane must own its center hit target",
    );
    return rect;
  });
  if (rects.length === 2) {
    assert(
      rects[0].right <= rects[1].left + 2 ||
        rects[1].right <= rects[0].left + 2,
      "split panes must not overlap",
    );
  }
  for (const tab of hidden) {
    const panel = panelFor(tab);
    const rect = panel.getBoundingClientRect();
    const hit = document.elementFromPoint(
      rect.x + rect.width / 2,
      rect.y + rect.height / 2,
    );
    assert(
      !hit || !panel.contains(hit),
      "inactive pane must not own a pointer hit",
    );
    if (visible.length === 2) {
      assertEquals(
        getComputedStyle(panel)!.position,
        "absolute",
        "inactive pane is outside split layout",
      );
      assertEquals(
        getComputedStyle(panel)!.visibility,
        "hidden",
        "inactive pane cannot receive pointer hits",
      );
    }
  }
  assertEquals(
    browser.selectedTab,
    visible[0],
    "layout must retain selected tab",
  );
}

/** A second split keeps the source in flex layout during the native browser swap. */
async function testSplitGroupRoundTrip(): Promise<void> {
  const browser = (window as unknown as AdoptionWindow).gBrowser;
  const originalTab = browser.selectedTab;
  const pref = "browser.tabs.splitView.enabled";
  const hadPref = Services.prefs.prefHasUserValue(pref);
  const previousPref = Services.prefs.getBoolPref(pref, false);
  const style = document.getElementById("floorp-split-view-styles") ??
    document.createElement("style");
  const originalStyles = style.textContent;
  const ownedStyle = !style.isConnected;
  style.textContent = splitViewStyles;
  if (ownedStyle) document.head.appendChild(style);
  const created: AdoptionTab[] = [];
  let destination: AdoptionWindow | undefined;
  try {
    Services.prefs.setBoolPref(pref, true);
    const firstTabs = createPair(browser, "retained", created);
    browser.addTabGroup(firstTabs, { label: "Retained split" });
    browser.addTabSplitView(firstTabs, { insertBefore: firstTabs[0] });
    const movedTabs = createPair(browser, "moved", created);
    const group = browser.addTabGroup(movedTabs, { label: "Moved split" });
    browser.addTabSplitView(movedTabs, { insertBefore: movedTabs[0] });
    browser.selectedTab = movedTabs[0];
    await waitUntil(
      () =>
        created.every((tab) =>
          tab.linkedBrowser.contentTitle.startsWith("QA ")
        ),
      "all source panes must finish loading before adoption",
    );
    const normal = browser.addTab("data:text/html,<title>QA normal</title>", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    created.push(normal);
    await assertPaneLayout(browser, firstTabs, [...movedTabs, normal]);
    await assertPaneLayout(browser, movedTabs, [...firstTabs, normal]);
    await assertPaneLayout(browser, [normal], [...firstTabs, ...movedTabs]);
    await assertPaneLayout(browser, movedTabs, [...firstTabs, normal]);
    const expectedTitles = movedTabs.map((tab) =>
      tab.linkedBrowser.contentTitle
    );
    const expectedUrls = movedTabs.map((tab) =>
      tab.linkedBrowser.currentURI.spec
    );
    destination = browser.replaceGroupWithWindow(group);
    await waitUntil(
      () => Boolean(destination?.gBrowser?.tabGroups[0]?.tabs.length === 2),
      "the split group should finish moving to its new window",
    );
    const movedGroup = destination.gBrowser.tabGroups[0];
    const destinationStyle = destination.document.getElementById(
      "floorp-split-view-styles",
    );
    if (destinationStyle) destinationStyle.textContent = splitViewStyles;
    await new Promise((resolve) => setTimeout(resolve, 500));
    assertEquals(
      movedGroup.tabs.map((tab) => tab.linkedBrowser.contentTitle).join("\n"),
      expectedTitles.join("\n"),
      "moving a split group must preserve both loaded documents",
    );
    assertEquals(
      movedGroup.tabs.map((tab) => tab.linkedBrowser.currentURI.spec).join(
        "\n",
      ),
      expectedUrls.join("\n"),
      "adoption must preserve both document URLs",
    );
    const returnedGroup = browser.adoptTabGroup(movedGroup, {
      tabIndex: browser.tabs.length,
    });
    created.push(...returnedGroup.tabs);
    await new Promise((resolve) => setTimeout(resolve, 500));
    assertEquals(
      returnedGroup.tabs.map((tab) => tab.linkedBrowser.contentTitle).join(
        "\n",
      ),
      expectedTitles.join("\n"),
      "moving the group back beside another split must preserve both documents",
    );
    assertEquals(
      returnedGroup.tabs.map((tab) => tab.linkedBrowser.currentURI.spec).join(
        "\n",
      ),
      expectedUrls.join("\n"),
      "adoption must preserve both document URLs",
    );
    await assertPaneLayout(browser, firstTabs, [...returnedGroup.tabs, normal]);
    await assertPaneLayout(browser, returnedGroup.tabs, [...firstTabs, normal]);
    await assertPaneLayout(browser, [normal], [
      ...firstTabs,
      ...returnedGroup.tabs,
    ]);
    await assertPaneLayout(browser, returnedGroup.tabs, [...firstTabs, normal]);
  } finally {
    if (originalTab.isConnected) browser.selectedTab = originalTab;
    if (destination && !destination.closed) destination.close();
    for (const tab of created) {
      if (tab.isConnected && browser.tabs.includes(tab)) {
        browser.removeTab(tab, { animate: false });
      }
    }
    if (ownedStyle) style.remove();
    else style.textContent = originalStyles;
    if (hadPref) Services.prefs.setBoolPref(pref, previousPref);
    else Services.prefs.clearUserPref(pref);
  }
}

function createPair(
  browser: AdoptionBrowser,
  prefix: string,
  created: AdoptionTab[],
): AdoptionTab[] {
  return Array.from({ length: 2 }, (_, index) => {
    const tab = browser.addTab(
      `data:text/html,<title>QA ${prefix} ${index}</title><h1>QA ${prefix} ${index}</h1>`,
      {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    created.push(tab);
    return tab;
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("splitViewAdoption.test.ts", [{
    name: "split group preserves loaded panes across a window round trip",
    fn: testSplitGroupRoundTrip,
  }]);
}
