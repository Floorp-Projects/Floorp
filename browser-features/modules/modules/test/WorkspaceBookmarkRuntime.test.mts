// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  BookmarkDialogWindow,
  BookmarkNode,
  BookmarkRuntime,
  BrowserForBookmarks,
} from "./WorkspaceBookmarkRuntime.types.ts";

function runtime(): BookmarkRuntime {
  const { WorkspaceBookmarks } = ChromeUtils.importESModule(
    "resource://noraneko/modules/WorkspaceBookmarks.sys.mjs",
  ) as { WorkspaceBookmarks: BookmarkRuntime["scopes"] };
  const { PlacesUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: BookmarkRuntime["places"] };
  const { PlacesUIUtils } = ChromeUtils.importESModule(
    "moz-src:///browser/components/places/PlacesUIUtils.sys.mjs",
  ) as { PlacesUIUtils: BookmarkRuntime["ui"] };
  return { scopes: WorkspaceBookmarks, places: PlacesUtils, ui: PlacesUIUtils };
}

async function testNativeBookmarkViewsAndSaveGuards(): Promise<void> {
  const { scopes, places, ui } = runtime();
  await scopes.ready;
  const browser = Services.wm.getMostRecentWindow("navigator:browser") as
    | BrowserForBookmarks
    | null;
  assert(browser, "browser window exists");
  const before = scopes.getContext(browser);
  const workspace = crypto.randomUUID();
  const otherWorkspace = crypto.randomUUID();
  const suffix = crypto.randomUUID();
  const oldPref = Services.prefs.getBoolPref(
    "floorp.workspaces.bookmarks.enabled",
    true,
  );
  const hadPref = Services.prefs.prefHasUserValue(
    "floorp.workspaces.bookmarks.enabled",
  );
  let rootGuid: string | undefined;
  let hiddenGuid: string | undefined;
  let menu: InstanceType<BrowserForBookmarks["PlacesMenu"]> | undefined;
  let menuElement: Element | undefined;
  try {
    Services.prefs.setBoolPref("floorp.workspaces.bookmarks.enabled", true);
    const root = await places.bookmarks.insert({
      parentGuid: places.bookmarks.unfiledGuid,
      type: places.bookmarks.TYPE_FOLDER,
      title: `Runtime scope ${suffix}`,
    });
    rootGuid = root.guid;
    const visibleFolder = await places.bookmarks.insert({
      parentGuid: root.guid,
      type: places.bookmarks.TYPE_FOLDER,
      title: "Visible nested folder",
    });
    const visible = await places.bookmarks.insert({
      parentGuid: visibleFolder.guid,
      title: `Needle ${suffix}`,
      url: `https://visible-${suffix}.invalid/`,
    });
    const hiddenFolder = await places.bookmarks.insert({
      parentGuid: root.guid,
      type: places.bookmarks.TYPE_FOLDER,
      title: "Hidden folder",
    });
    hiddenGuid = hiddenFolder.guid;
    await places.bookmarks.insert({
      parentGuid: hiddenFolder.guid,
      title: `Needle ${suffix}`,
      url: `https://hidden-${suffix}.invalid/`,
    });
    await scopes.refresh();
    await scopes.setFolderScope(hiddenFolder.guid, [otherWorkspace]);
    scopes.setWindowContext(browser, workspace, true);

    const query = places.history.getNewQuery();
    query.setParents([root.guid]);
    query.searchTerms = `Needle ${suffix}`;
    const options = places.history.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS!;
    options.maxResults = 1;
    const prepared = ui.workspaceBookmarkQuery(query, options, browser);
    assertEquals(
      prepared.query.parentCount,
      2,
      "recursive query avoids folder fast path",
    );
    assertEquals(
      prepared.options.maxResults,
      0,
      "native limit cannot consume hidden entries",
    );
    const result = places.history.executeQuery(
      prepared.query,
      prepared.options,
    );
    result.root.containerOpen = true;
    try {
      const shown: string[] = [];
      for (let i = 0; i < result.root.childCount; i++) {
        const node = result.root.getChild(i);
        if (
          ui.isWorkspaceBookmarkNodeVisible(node, browser) &&
          ui.workspaceBookmarkMatches(node, prepared.searchTerms)
        ) {
          shown.push(node.bookmarkGuid);
        }
      }
      assertEquals(
        shown.join(","),
        visible.guid,
        "recursive search keeps the visible nested match",
      );
    } finally {
      result.root.containerOpen = false;
    }

    menuElement = browser.document.createXULElement("menu");
    const popup = browser.document.createXULElement("menupopup");
    menuElement.appendChild(popup);
    browser.document.documentElement.appendChild(menuElement);
    const mainPopup = browser.document.getElementById("bookmarksMenuPopup");
    assert(mainPopup, "native bookmarks menu exists");
    mainPopup.dispatchEvent(
      new browser.Event("popupshowing", { bubbles: true }),
    );
    const mainMenu = browser.document.getElementById("bookmarksMenu") as
      & Element
      & {
        _placesView?: object;
      };
    assert(
      mainMenu._placesView,
      "native menu event initializes its Places view",
    );
    const Menu = Reflect.get(
      mainMenu._placesView,
      "constructor",
    ) as BrowserForBookmarks["PlacesMenu"];
    menu = new Menu(
      { target: popup, originalTarget: popup },
      `place:parent=${root.guid}&queryType=1&sort=12&maxResults=1`,
    );
    const visibleMenuGuids = () =>
      Array.from(popup.children)
        .map((child) =>
          (child as Element & { _placesNode?: BookmarkNode })._placesNode
            ?.bookmarkGuid
        )
        .filter(Boolean).join(",");
    assertEquals(
      visibleMenuGuids(),
      visible.guid,
      "menu filters GUIDs before applying recent limit",
    );
    menu._nativeView = true;
    menu._rebuildPopup(popup);
    assertEquals(
      visibleMenuGuids(),
      visible.guid,
      "native menu shares the same visibility filter",
    );

    const state = new ui.BookmarkState({
      info: {
        itemGuid: visible.guid,
        parentGuid: visible.parentGuid,
        title: visible.title,
      },
    });
    state.setWorkspaceWindow(browser);
    await state._titleChanged("Must not save after workspace switch");
    scopes.setWindowContext(browser, otherWorkspace, true);
    scopes.setWindowContext(browser, workspace, true);
    assertEquals(
      await state.save(),
      undefined,
      "A to B to A switch cancels an open editor",
    );
    assertEquals(
      (await places.bookmarks.fetch(visible.guid))?.title,
      visible.title,
      "cancelled save preserves the stored title",
    );
  } finally {
    menu?.uninit();
    menuElement?.remove();
    if (hiddenGuid) await scopes.setFolderScope(hiddenGuid, null);
    if (rootGuid) await places.bookmarks.remove(rootGuid);
    await scopes.refresh();
    scopes.setWindowContext(browser, before.workspaceId, before.enabled);
    if (hadPref) {
      Services.prefs.setBoolPref(
        "floorp.workspaces.bookmarks.enabled",
        oldPref,
      );
    } else Services.prefs.clearUserPref("floorp.workspaces.bookmarks.enabled");
  }
}

async function waitForBookmarkUI(
  predicate: () => boolean | Promise<boolean>,
  message: string,
): Promise<void> {
  for (let attempt = 0; attempt < 200; attempt++) {
    if (await predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(message);
}

async function prepareStarUIAnchor(
  browser: BrowserForBookmarks,
): Promise<() => void> {
  const toolbar = browser.document.getElementById("nav-bar") as
    | XULElement
    | null;
  assert(toolbar, "the native navigation toolbar exists");
  const wasVisible = !toolbar.hasAttribute("collapsed");
  const wasInstant = toolbar.classList.contains("instant");
  const restore = () => {
    try {
      browser.setToolbarVisibility(toolbar, wasVisible, false, false);
    } finally {
      toolbar.classList.toggle("instant", wasInstant);
    }
  };
  browser.setToolbarVisibility(toolbar, true, false, false);
  browser.focus();
  try {
    await waitForBookmarkUI(
      () =>
        ["star-button-box", "pageActionButton", "identity-icon"].some((id) => {
          const anchor = browser.document.getElementById(id) as
            | XULElement
            | null;
          if (!anchor || anchor.hidden) return false;
          // Native PageActions deliberately reads bounds without flushing. Wait
          // for actual toolbar layout before asking the real StarUI to anchor.
          const bounds = anchor.getBoundingClientRect();
          return bounds.width > 0 && bounds.height > 0;
        }),
      "the real bookmark panel has a laid-out native anchor",
    );
    return restore;
  } catch (error) {
    restore();
    throw error;
  }
}

async function testRealStarWorkspaceSaveAndCopySelection(): Promise<void> {
  const { scopes, places, ui } = runtime();
  await scopes.ready;
  const browser = Services.wm.getMostRecentWindow("navigator:browser") as
    | BrowserForBookmarks
    | null;
  assert(browser, "browser window exists");
  const before = scopes.getContext(browser);
  const workspace = crypto.randomUUID();
  const otherWorkspace = crypto.randomUUID();
  const suffix = crypto.randomUUID();
  const url = new URL(`https://star-${suffix}.invalid/`);
  let workspaceFolder: string | undefined;
  let fixtureFolder: string | undefined;
  let hiddenFolder: string | undefined;
  let restoreToolbar: (() => void) | undefined;
  const cleanupGuids: string[] = [];
  try {
    restoreToolbar = await prepareStarUIAnchor(browser);
    scopes.setWindowContext(browser, workspace, true);
    const node = await ui.promiseNodeLikeFromFetchInfo({
      guid: places.bookmarks.unsavedGuid,
      parentGuid: places.bookmarks.unfiledGuid,
      title: "New workspace bookmark",
      url,
    });
    await browser.StarUI.showEditBookmarkPopup(node, true, url);
    await waitForBookmarkUI(
      () =>
        browser.document.getElementById(
          "floorp-save-bookmark-for-workspace",
        ) !== null,
      "real StarUI exposes the workspace save action",
    );
    await waitForBookmarkUI(() =>
      Boolean(
        browser.document.getElementById(
          "floorp-bookmark-destination-scope",
        )?.textContent?.trim(),
      ), "the real save panel explains its destination and effective scope");
    const button = browser.document.getElementById(
      "floorp-save-bookmark-for-workspace",
    ) as HTMLButtonElement;
    button.click();
    await waitForBookmarkUI(async () => {
      const records = await scopes.visibleBookmarksForUrl(url.href, browser);
      if (!records.length) return false;
      cleanupGuids.push(records[0].guid);
      workspaceFolder = records[0].parentGuid ?? undefined;
      return browser.StarUI.panel.state === "closed";
    }, "workspace save action commits and closes the actual bookmark editor");
    assert(workspaceFolder, "workspace save creates a destination folder");
    assertEquals(
      scopes.getScope(workspaceFolder)?.join(","),
      workspace,
      "the saved bookmark inherits only the selected workspace",
    );
    assertEquals(
      await scopes.getDefaultFolder(browser),
      workspaceFolder,
      "the committed destination becomes this workspace's remembered folder",
    );

    const fixture = await places.bookmarks.insert({
      parentGuid: places.bookmarks.unfiledGuid,
      type: places.bookmarks.TYPE_FOLDER,
      title: "Star duplicate fixture",
    });
    fixtureFolder = fixture.guid;
    const otherVisible = await places.bookmarks.insert({
      parentGuid: fixture.guid,
      title: "Explicitly selected visible copy",
      url: url.href,
    });
    const hidden = await places.bookmarks.insert({
      parentGuid: fixture.guid,
      type: places.bookmarks.TYPE_FOLDER,
      title: "Hidden duplicate folder",
    });
    hiddenFolder = hidden.guid;
    const hiddenCopy = await places.bookmarks.insert({
      parentGuid: hidden.guid,
      title: "Must survive removal",
      url: url.href,
    });
    await scopes.refresh();
    await scopes.setFolderScope(hidden.guid, [otherWorkspace]);
    const editNode = await ui.promiseNodeLikeFromFetchInfo({
      guid: cleanupGuids[0],
      parentGuid: workspaceFolder,
      title: "New workspace bookmark",
      url,
    });
    await browser.StarUI.showEditBookmarkPopup(editNode, false, url);
    await waitForBookmarkUI(
      () =>
        browser.document.querySelector(
          "#floorp-workspace-bookmark-controls select",
        ) !== null,
      "same URL visible copies get an explicit chooser",
    );
    const chooser = browser.document.querySelector(
      "#floorp-workspace-bookmark-controls select",
    ) as HTMLSelectElement;
    assertEquals(
      chooser.options.length,
      2,
      "the chooser excludes the hidden same-URL bookmark",
    );
    chooser.value = otherVisible.guid;
    chooser.dispatchEvent(new browser.Event("change", { bubbles: true }));
    await waitForBookmarkUI(() => {
      const overlay = Reflect.get(browser, "gEditItemOverlay") as {
        bookmarkState?: { _guid?: string } | null;
      };
      return browser.StarUI.selectedWorkspaceBookmarkGuid ===
          otherVisible.guid &&
        overlay.bookmarkState?._guid === otherVisible.guid;
    }, "the star editor finishes switching to the explicitly selected GUID");
    await waitForBookmarkUI(
      () =>
        Boolean(
          browser.document.getElementById(
            "floorp-bookmark-destination-scope",
          )?.textContent?.includes("Star duplicate fixture"),
        ),
      "the destination and effective scope display follows the selected copy's parent",
    );
    browser.StarUI.removeBookmarkButtonCommand();
    await waitForBookmarkUI(
      async () => await places.bookmarks.fetch(otherVisible.guid) === null,
      "remove button removes the selected visible GUID",
    );
    assert(
      await places.bookmarks.fetch(cleanupGuids[0]),
      "another visible same-URL bookmark survives",
    );
    assert(
      await places.bookmarks.fetch(hiddenCopy.guid),
      "the hidden same-URL bookmark survives",
    );
  } finally {
    try {
      if (browser.StarUI.panel.state !== "closed") {
        browser.StarUI._cancelOnPopupHidden = true;
        browser.StarUI.panel.hidePopup();
        await waitForBookmarkUI(
          () => browser.StarUI.panel.state === "closed",
          "star editor closes during cleanup",
        );
      }
      for (const guid of new Set(cleanupGuids)) {
        await places.bookmarks.remove(guid);
      }
      if (hiddenFolder) await scopes.setFolderScope(hiddenFolder, null);
      if (fixtureFolder) await places.bookmarks.remove(fixtureFolder);
      await scopes.refresh();
      if (workspaceFolder) {
        await scopes.releaseCreatedWorkspaceFolder({
          guid: workspaceFolder,
          created: true,
          workspaceId: workspace,
        });
      }
    } finally {
      scopes.setWindowContext(browser, before.workspaceId, before.enabled);
      restoreToolbar?.();
    }
  }
}

async function testBookmarkPropertiesCancelAcrossContexts(): Promise<void> {
  const { scopes, places, ui } = runtime();
  await scopes.ready;
  const browser = Services.wm.getMostRecentWindow("navigator:browser") as
    | BrowserForBookmarks
    | null;
  assert(browser, "browser window exists");
  const before = scopes.getContext(browser);
  const workspace = crypto.randomUUID();
  const otherWorkspace = crypto.randomUUID();
  const dialogURL = "chrome://browser/content/places/bookmarkProperties.xhtml";
  let folderGuid: string | undefined;
  let standalone: BookmarkDialogWindow | null = null;
  let pending: Promise<string | undefined> | undefined;
  try {
    const folder = await places.bookmarks.insert({
      parentGuid: places.bookmarks.unfiledGuid,
      type: places.bookmarks.TYPE_FOLDER,
      title: "Real properties dialog fixture",
    });
    folderGuid = folder.guid;
    const bookmark = await places.bookmarks.insert({
      parentGuid: folder.guid,
      title: "Original dialog title",
      url: `https://dialog-${crypto.randomUUID()}.invalid/`,
    });
    await scopes.refresh();
    scopes.setWindowContext(browser, workspace, true);
    const node = await ui.promiseNodeLikeFromFetchInfo({
      guid: bookmark.guid,
      parentGuid: folder.guid,
      title: bookmark.title,
      url: new URL(`https://dialog-fixture.invalid/${bookmark.guid}`),
    });
    pending = ui.showBookmarkDialog({ action: "edit", node }, browser);
    await waitForBookmarkUI(
      () => {
        const dialog = browser.gDialogBox.dialog?._frame.contentWindow;
        return dialog?.document.documentURI === dialogURL &&
          Boolean(
            dialog.gEditItemOverlay?.bookmarkState &&
              dialog.gEditItemOverlay?._floorpWorkspaceObserver,
          );
      },
      "the actual bookmark properties subdialog is ready and observes its workspace",
    );
    const subdialog = browser.gDialogBox.dialog?._frame.contentWindow;
    assert(
      subdialog?.gEditItemOverlay?.bookmarkState,
      "subdialog has native editing state",
    );
    await subdialog.gEditItemOverlay.bookmarkState._titleChanged(
      "Must not save across workspace switch",
    );
    scopes.setWindowContext(browser, otherWorkspace, true);
    await waitForBookmarkUI(
      () => !browser.gDialogBox.isOpen,
      "switching workspace cancels and closes the real properties subdialog",
    );
    assertEquals(
      await pending,
      undefined,
      "cancelled subdialog does not commit",
    );
    pending = undefined;
    assertEquals(
      (await places.bookmarks.fetch(bookmark.guid))?.title,
      bookmark.title,
      "subdialog cancellation preserves the original bookmark",
    );

    scopes.setWindowContext(browser, workspace, true);
    const openDialog = Reflect.get(browser, "openDialog") as (
      url: string,
      name: string,
      features: string,
      args: Record<string, unknown>,
    ) => BookmarkDialogWindow;
    standalone = openDialog.call(
      browser,
      dialogURL,
      "_blank",
      "chrome,resizable=no",
      { action: "edit", node },
    );
    await waitForBookmarkUI(
      () =>
        Boolean(
          standalone?.document.documentURI === dialogURL &&
            standalone.gEditItemOverlay?.bookmarkState &&
            standalone.gEditItemOverlay?._floorpWorkspaceObserver,
        ),
      "the standalone native bookmark properties editor is ready",
    );
    assert(
      standalone.gEditItemOverlay?.bookmarkState,
      "standalone dialog has editing state",
    );
    await standalone.gEditItemOverlay.bookmarkState._titleChanged(
      "Must not save an invisible target",
    );
    await scopes.setFolderScope(folder.guid, [otherWorkspace]);
    await waitForBookmarkUI(
      () => Boolean(standalone?.closed),
      "making the edited GUID invisible cancels the exact standalone properties dialog",
    );
    assertEquals(
      (await places.bookmarks.fetch(bookmark.guid))?.title,
      bookmark.title,
      "standalone cancellation preserves the original bookmark",
    );
  } finally {
    browser.gDialogBox.dialog?.abort();
    if (pending) await pending;
    if (standalone && !standalone.closed) standalone.close();
    if (folderGuid) {
      await scopes.setFolderScope(folderGuid, null);
      await places.bookmarks.remove(folderGuid);
    }
    await scopes.refresh();
    scopes.setWindowContext(browser, before.workspaceId, before.enabled);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkspaceBookmarkRuntime.test.mts", [{
    name:
      "native recursive views, visible limits, Mac menu path and stale saves",
    fn: testNativeBookmarkViewsAndSaveGuards,
  }, {
    name:
      "actual StarUI saves to workspace and removes only an explicitly selected visible copy",
    fn: testRealStarWorkspaceSaveAndCopySelection,
  }, {
    name:
      "actual bookmark properties subdialog and standalone editors cancel stale contexts",
    fn: testBookmarkPropertiesCancelAcrossContexts,
  }]);
}
