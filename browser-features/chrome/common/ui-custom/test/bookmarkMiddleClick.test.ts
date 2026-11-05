// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { handleBookmarkMiddleClick } from "../layout/bookmark-middle-click.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function checkDetachedBookmark(folder: boolean, background: boolean) {
  const { PlacesUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  );
  const bar = document.getElementById("PersonalToolbar");
  assert(bar?.parentNode, "the bookmarks toolbar exists");
  const parent = bar.parentNode;
  const sibling = bar.nextSibling;
  const label = `Middle click regression ${Date.now()}`;
  const previousTabs = new Set(gBrowser.tabs);
  const previousSelected = gBrowser.selectedTab;
  const pref = "browser.tabs.loadBookmarksInBackground";
  const hadPref = Services.prefs.prefHasUserValue(pref);
  const oldPref = Services.prefs.getBoolPref(pref);
  const oldCollapsed = bar.getAttribute("collapsed");
  const fixture = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: label,
    ...(folder
      ? { type: PlacesUtils.bookmarks.TYPE_FOLDER }
      : { url: "https://example.invalid/kan100-bookmark" }),
  });
  const handler = (event: Event) => handleBookmarkMiddleClick(event, bar);
  try {
    if (folder) {
      for (let i = 0; i < 2; i++) {
        await PlacesUtils.bookmarks.insert({
          parentGuid: fixture.guid,
          title: `Child ${i}`,
          url: `https://example.invalid/kan100-folder-${i}`,
        });
      }
    }
    Services.prefs.setBoolPref(pref, background);
    bar.removeAttribute("collapsed");
    const deadline = Date.now() + 10_000;
    let button: Element | undefined;
    while (!button && Date.now() < deadline) {
      button = Array.from(bar.querySelectorAll(".bookmark-item")).find(
        (item) => item.getAttribute("label") === label,
      );
      if (!button) await new Promise((resolve) => setTimeout(resolve, 25));
    }
    assert(button, "Places renders the new bookmark");

    // Reproduce the missing native toolbox ancestor, retaining the live Places
    // view. Listen at both capture sites used by the layout forwarder.
    document.documentElement.appendChild(bar);
    document.addEventListener("click", handler, true);
    bar.addEventListener("click", handler, true);
    await new Promise((resolve) => setTimeout(resolve, 50));
    const rect = button.getBoundingClientRect();
    assert(rect.width > 0 && rect.height > 0, "bookmark is visible");
    const x = rect.x + rect.width / 2;
    const y = rect.y + rect.height / 2;
    const targetWindow = window as unknown as {
      synthesizeMouseEvent(
        type: string,
        x: number,
        y: number,
        event: { button: number; clickCount: number },
      ): void;
    };
    targetWindow.synthesizeMouseEvent("mousedown", x, y, {
      button: 1,
      clickCount: 1,
    });
    targetWindow.synthesizeMouseEvent("mouseup", x, y, {
      button: 1,
      clickCount: 1,
    });
    await new Promise((resolve) => setTimeout(resolve, 250));
    const opened = gBrowser.tabs.filter((tab) => !previousTabs.has(tab));
    assertEquals(opened.length, folder ? 2 : 1, "open each bookmark once");
    if (!folder) {
      assertEquals(
        gBrowser.selectedTab === previousSelected,
        background,
        "retain native bookmark foreground/background behavior",
      );
    }
  } finally {
    document.removeEventListener("click", handler, true);
    bar.removeEventListener("click", handler, true);
    parent.insertBefore(bar, sibling);
    if (oldCollapsed === null) bar.removeAttribute("collapsed");
    else bar.setAttribute("collapsed", oldCollapsed);
    for (const tab of [...gBrowser.tabs]) {
      if (!previousTabs.has(tab)) gBrowser.removeTab(tab);
    }
    gBrowser.selectedTab = previousSelected;
    if (hadPref) Services.prefs.setBoolPref(pref, oldPref);
    else Services.prefs.clearUserPref(pref);
    await PlacesUtils.bookmarks.remove(fixture.guid);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("bookmarkMiddleClick.test.ts", [
    {
      name: "detached middle-click opens one background bookmark tab",
      fn: () => checkDetachedBookmark(false, true),
    },
    {
      name: "detached middle-click respects foreground bookmark preference",
      fn: () => checkDetachedBookmark(false, false),
    },
    {
      name: "detached folder middle-click opens each child once",
      fn: () => checkDetachedBookmark(true, true),
    },
  ]);
}
