// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CHROME_EXTRAS_SIDEBAR_HEADER_PREF } from "../chrome-extras.ts";
import {
  config,
  getChromeExtrasSettings,
  setConfig,
  updateChromeExtrasSetting,
} from "../configs.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type {
  BookmarksPanel,
  ExtensionBrowser,
  HistoryPanel,
  LitElement,
  SearchInput,
  SidebarMenu,
  SidebarTestController,
  TemporaryAddon,
  TestAddonManager,
  TestPlacesUtils,
} from "./sidebar-header-test-types.ts";

declare const SidebarController: SidebarTestController;

const { PlacesUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesUtils.sys.mjs",
) as { PlacesUtils: TestPlacesUtils };

async function waitFor(check: () => boolean, message: string): Promise<void> {
  const deadline = Date.now() + 10_000;
  while (!check()) {
    assert(Date.now() < deadline, message);
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

function display(element: Element): string {
  const win = element.ownerDocument.defaultView;
  assert(win, "element must have a window");
  const style = win.getComputedStyle(element);
  assert(style, "connected element must have computed styles");
  return style.display;
}

async function setHidden(hidden: boolean): Promise<void> {
  updateChromeExtrasSetting("hiddenSidebarHeader", hidden);
  await waitFor(
    () =>
      Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, !hidden) ===
        hidden,
    "the saved toggle must reach the Runtime preference",
  );
}

async function withRevampedSidebar(test: () => Promise<void>): Promise<void> {
  await SidebarController.promiseInitialized;
  const before = config();
  const pref = "sidebar.revamp";
  const hadUserPref = Services.prefs.prefHasUserValue(pref);
  const previousPref = Services.prefs.getBoolPref(pref, false);
  const bookmarksPref = "sidebar.updatedBookmarks.enabled";
  const hadBookmarksPref = Services.prefs.prefHasUserValue(bookmarksPref);
  const previousBookmarksPref = Services.prefs.getBoolPref(
    bookmarksPref,
    false,
  );
  const previousID = SidebarController.currentID;
  const wasOpen = SidebarController.isOpen;
  const previousHistoryURL = SidebarController.sidebars.get(
    "viewHistorySidebar",
  )?.url;
  const previousBookmarksURL = SidebarController.sidebars.get(
    "viewBookmarksSidebar",
  )?.url;
  try {
    Services.prefs.setBoolPref(bookmarksPref, true);
    Services.prefs.setBoolPref(pref, true);
    await waitFor(
      () =>
        SidebarController.sidebars.get("viewHistorySidebar")?.url.endsWith(
            "sidebar-history.html",
          ) === true &&
        SidebarController.sidebars.get("viewBookmarksSidebar")?.url.endsWith(
            "sidebar-bookmarks.html",
          ) === true,
      "revamped History and Bookmarks must become available",
    );
    await setHidden(false);
    await test();
  } finally {
    (document.getElementById("sidebar-history-menu") as SidebarMenu | null)
      ?.hidePopup();
    SidebarController.hide();
    setConfig(before);
    if (hadUserPref) Services.prefs.setBoolPref(pref, previousPref);
    else Services.prefs.clearUserPref(pref);
    if (hadBookmarksPref) {
      Services.prefs.setBoolPref(bookmarksPref, previousBookmarksPref);
    } else Services.prefs.clearUserPref(bookmarksPref);
    await waitFor(
      () =>
        SidebarController.sidebars.get("viewHistorySidebar")?.url ===
          previousHistoryURL &&
        SidebarController.sidebars.get("viewBookmarksSidebar")?.url ===
          previousBookmarksURL,
      "restore the original sidebar implementation",
    );
    if (wasOpen && previousID && SidebarController.sidebars.has(previousID)) {
      await SidebarController.show(previousID);
    }
  }
}

async function showPanel<T extends LitElement>(
  id: string,
  selector: string,
): Promise<T> {
  await SidebarController.show(id);
  await waitFor(
    () =>
      !!SidebarController.browser.contentDocument.querySelector(selector)
        ?.shadowRoot
        ?.querySelector("sidebar-panel-header")?.shadowRoot,
    `${id}: the real panel and header must finish rendering`,
  );
  const page = SidebarController.browser.contentDocument.querySelector<T>(
    selector,
  );
  assert(page, `${id}: panel must exist`);
  await page.updateComplete;
  return page;
}

function heading(page: LitElement): Element {
  const element = page.shadowRoot?.querySelector("sidebar-panel-header")
    ?.shadowRoot
    ?.querySelector(".sidebar-panel-heading");
  assert(element, "the panel must contain the real header title row");
  return element;
}

async function search(page: LitElement, query: string): Promise<void> {
  const input = page.shadowRoot?.querySelector<SearchInput>("moz-input-search");
  assert(input, "the panel must keep its search input");
  await input.updateComplete;
  assert(
    input.getBoundingClientRect().height > 0 &&
      input.inputEl.getBoundingClientRect().height > 0,
    "search must remain visible after hiding the title",
  );
  input.inputEl.focus();
  assertEquals(
    input.shadowRoot?.activeElement,
    input.inputEl,
    "the native search input must accept focus",
  );
  input.inputEl.value = query;
  // Exercise the input's real debounce and search event, not the controller.
  input.inputEl.dispatchEvent(
    new Event("input", { bubbles: true, composed: true }),
  );
}

async function testEveryDesignMirrorsOnlyTheSavedToggle(): Promise<void> {
  const before = config();
  try {
    for (
      const design of [
        "lepton",
        "photon",
        "protonfix",
        "proton",
        "fluerial",
      ] as const
    ) {
      Services.prefs.setBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, true);
      setConfig((prev) => ({
        ...prev,
        globalConfigs: { ...prev.globalConfigs, userInterface: design },
        uiCustomization: {
          ...prev.uiCustomization,
          chromeExtras: {
            ...getChromeExtrasSettings(),
            hiddenSidebarHeader: false,
          },
        },
      }));
      await waitFor(
        () =>
          !Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, true),
        `${design}: saved false must overwrite a stale mirror`,
      );
      await setHidden(true);
    }
  } finally {
    setConfig(before);
    await waitFor(
      () =>
        Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, false) ===
          getChromeExtrasSettings().hiddenSidebarHeader,
      "restoring the config must restore the derived mirror",
    );
  }
}

async function testHistorySearchAndOptions(): Promise<void> {
  const token = `floorp-header-${crypto.randomUUID()}`;
  const url = `https://example.com/${token}`;
  const sortPref = "sidebar.history.sortOption";
  const hadSortPref = Services.prefs.prefHasUserValue(sortPref);
  const previousSort = Services.prefs.getStringPref(sortPref, "date");
  await PlacesUtils.history.insert({
    url,
    title: token,
    visits: [{ date: new Date() }],
  });
  try {
    await withRevampedSidebar(async () => {
      const page = await showPanel<HistoryPanel>(
        "viewHistorySidebar",
        "sidebar-history",
      );
      const title = heading(page);
      await waitFor(
        () => display(title) === "flex",
        "History title is initially visible",
      );
      await setHidden(true);
      await waitFor(() => display(title) === "none", "History title must hide");
      await search(page, token);
      await waitFor(
        () =>
          page.controller.searchQuery === token &&
          Array.from(page.lists).flatMap((list) => Array.from(list.rowEls))
            .some((row) => row.mainEl?.href === url),
        "hidden-header History must filter to the inserted visit",
      );
      await search(page, `${token}-absent`);
      await waitFor(
        () =>
          page.controller.searchQuery === `${token}-absent` &&
          Array.from(page.lists).some((list) => !!list.emptyState),
        "History must render an empty search result",
      );
      await search(page, "");
      await waitFor(
        () => page.controller.searchQuery === "",
        "History search must clear",
      );
      const options = page.shadowRoot?.querySelector<HTMLElement>(
        ".menu-button",
      );
      const menu = document.getElementById("sidebar-history-menu") as
        | SidebarMenu
        | null;
      const sortItem = document.getElementById("sidebar-history-sort-by-site");
      assert(
        options && menu && sortItem,
        "History options and sort menu must exist",
      );
      assert(
        options.getBoundingClientRect().height > 0,
        "History options stay visible",
      );
      options.click();
      await waitFor(
        () => menu.state === "open",
        "History options must open with its title hidden",
      );
      menu.activateItem(sortItem);
      await waitFor(
        () => page.controller.sortOption === "site",
        "History sorting must still work",
      );
      menu.hidePopup();
      await setHidden(false);
      await waitFor(
        () => display(title) === "flex",
        "History title must restore without reopening",
      );
      await setHidden(true);
      SidebarController.hide();
      const reopened = await showPanel<HistoryPanel>(
        "viewHistorySidebar",
        "sidebar-history",
      );
      await waitFor(
        () => display(heading(reopened)) === "none",
        "reopened History inherits the toggle",
      );
      await search(reopened, token);
      await waitFor(
        () =>
          reopened.controller.searchQuery === token &&
          Array.from(reopened.lists).flatMap((list) => Array.from(list.rowEls))
            .some((row) => row.mainEl?.href === url),
        "reopened History still searches",
      );
    });
  } finally {
    if (hadSortPref) Services.prefs.setStringPref(sortPref, previousSort);
    else Services.prefs.clearUserPref(sortPref);
    await PlacesUtils.history.remove(url);
  }
}

async function testBookmarksSearchAndReopen(): Promise<void> {
  const token = `floorp-header-${crypto.randomUUID()}`;
  const bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: token,
    url: `https://example.com/${token}`,
  });
  try {
    await withRevampedSidebar(async () => {
      const page = await showPanel<BookmarksPanel>(
        "viewBookmarksSidebar",
        "sidebar-bookmarks",
      );
      const title = heading(page);
      await waitFor(
        () => display(title) === "flex",
        "Bookmarks title is initially visible",
      );
      await setHidden(true);
      await waitFor(
        () => display(title) === "none",
        "Bookmarks title must hide",
      );
      await search(page, token);
      await waitFor(
        () =>
          page.searchQuery === token && page.searchResults.length === 1 &&
          page.searchResults[0].guid === bookmark.guid,
        "hidden-header Bookmarks must find the inserted bookmark",
      );
      await search(page, `${token}-absent`);
      await waitFor(
        () =>
          page.searchQuery === `${token}-absent` &&
          page.searchResults.length === 0,
        "Bookmarks must filter unmatched entries",
      );
      await setHidden(false);
      await waitFor(
        () => display(title) === "flex",
        "Bookmarks title must restore",
      );
      assertEquals(
        page.searchQuery,
        `${token}-absent`,
        "changing the header must preserve the query",
      );
      await search(page, "");
      await waitFor(
        () => page.searchQuery === "",
        "Bookmarks search must clear",
      );
      await setHidden(true);
      SidebarController.hide();
      const reopened = await showPanel<BookmarksPanel>(
        "viewBookmarksSidebar",
        "sidebar-bookmarks",
      );
      await waitFor(
        () => display(heading(reopened)) === "none",
        "reopened Bookmarks inherits the toggle",
      );
      await search(reopened, token);
      await waitFor(
        () =>
          reopened.searchResults.some((item) => item.guid === bookmark.guid),
        "reopened Bookmarks still searches",
      );
    });
  } finally {
    await PlacesUtils.bookmarks.remove(bookmark.guid);
  }
}

async function testRealExtensionPanelHeader(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as { AddonManager: TestAddonManager };
  const directory = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "floorp-sidebar-header-",
  );
  const extensionID =
    `floorp-sidebar-header-${crypto.randomUUID()}@tests.invalid`;
  const sidebarID = `${
    extensionID.replace(/[^a-z0-9_-]/g, "_")
  }-sidebar-action`;
  let addon: TemporaryAddon | undefined;
  try {
    await IOUtils.writeUTF8(
      PathUtils.join(directory, "manifest.json"),
      JSON.stringify({
        manifest_version: 2,
        name: "Floorp sidebar header regression",
        version: "1.0",
        browser_specific_settings: { gecko: { id: extensionID } },
        sidebar_action: {
          default_title: "Header regression",
          default_panel: "sidebar.html",
          open_at_install: false,
        },
      }),
    );
    await IOUtils.writeUTF8(
      PathUtils.join(directory, "sidebar.html"),
      "<!doctype html><meta charset=utf-8><title>Sidebar header regression content</title><p>Extension content remains visible</p>",
    );
    await withRevampedSidebar(async () => {
      const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.initWithPath(directory);
      addon = await AddonManager.installTemporaryAddon(file);
      try {
        await waitFor(
          () => SidebarController.sidebars.has(sidebarID),
          "temporary extension must register its sidebar",
        );
        await SidebarController.show(sidebarID);
        await waitFor(
          () =>
            !!SidebarController.browser.contentDocument.getElementById(
              "sidebar-panel-header",
            )?.shadowRoot,
          "the actual webext-panels document must render its header",
        );
        const doc = SidebarController.browser.contentDocument;
        assert(
          doc.documentURI.endsWith("webext-panels.xhtml"),
          "test the real extension panel document",
        );
        const header = doc.getElementById("sidebar-panel-header") as LitElement;
        await header.updateComplete;
        const content = doc.getElementById("webext-panels-browser") as
          | ExtensionBrowser
          | null;
        assert(content, "the real extension content browser must exist");
        await waitFor(
          () => content.contentTitle === "Sidebar header regression content",
          "the extension document must load successfully",
        );
        await waitFor(
          () => header.getBoundingClientRect().height > 0,
          "extension title is initially visible",
        );
        const contentURI = content.currentURI.spec;
        await setHidden(true);
        await waitFor(
          () => display(header) === "none",
          "the empty extension host must collapse",
        );
        assertEquals(
          header.getBoundingClientRect().height,
          0,
          "hidden extension header must leave no space",
        );
        assert(
          content.getBoundingClientRect().height > 0,
          "extension content remains visible",
        );
        assertEquals(
          content.currentURI.spec,
          contentURI,
          "toggling the header must preserve the extension page",
        );
        await setHidden(false);
        await waitFor(
          () => header.getBoundingClientRect().height > 0,
          "extension header must restore in place",
        );
        assertEquals<Element | null>(
          doc.getElementById("webext-panels-browser"),
          content,
          "toggling must preserve the content browser",
        );
        await setHidden(true);
        SidebarController.hide();
        await SidebarController.show(sidebarID);
        await waitFor(() => {
          const reloaded = SidebarController.browser.contentDocument
            .getElementById("sidebar-panel-header");
          return !!reloaded?.shadowRoot && display(reloaded) === "none";
        }, "a reopened extension panel must inherit the enabled toggle");
        await setHidden(false);
        await waitFor(() => {
          const reloaded = SidebarController.browser.contentDocument
            .getElementById("sidebar-panel-header");
          return !!reloaded && reloaded.getBoundingClientRect().height > 0;
        }, "the reopened extension header must still respond to changes");
      } finally {
        SidebarController.hide();
        await addon?.uninstall();
        addon = undefined;
      }
    });
  } finally {
    await addon?.uninstall();
    await IOUtils.remove(directory, { recursive: true });
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("sidebar-header.test.ts", [
    {
      name: "every design mirrors the canonical toggle",
      fn: testEveryDesignMirrorsOnlyTheSavedToggle,
    },
    {
      name: "hidden History headers preserve search and options",
      fn: testHistorySearchAndOptions,
    },
    {
      name: "hidden Bookmarks headers preserve search after reopening",
      fn: testBookmarksSearchAndReopen,
    },
    {
      name: "real extension panel headers collapse and restore",
      fn: testRealExtensionPanelHeader,
    },
  ]);
}

await runAllTests();
