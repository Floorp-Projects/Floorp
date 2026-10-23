// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  TransferBookmark,
  TransferItem,
  TransferRuntime,
} from "./WorkspaceBookmarkTransfers.types.ts";

function runtime(): TransferRuntime {
  const { WorkspaceBookmarks: scopes } = ChromeUtils.importESModule(
    "resource://noraneko/modules/WorkspaceBookmarks.sys.mjs",
  ) as { WorkspaceBookmarks: TransferRuntime["scopes"] };
  const { PlacesUtils: places } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: TransferRuntime["places"] };
  const { PlacesUIUtils: ui } = ChromeUtils.importESModule(
    "moz-src:///browser/components/places/PlacesUIUtils.sys.mjs",
  ) as { PlacesUIUtils: TransferRuntime["ui"] };
  const { PlacesTransactions: transactions } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesTransactions.sys.mjs",
  ) as { PlacesTransactions: TransferRuntime["transactions"] };
  return { scopes, places, ui, transactions };
}

async function testRealTransferCopyMoveUndo(): Promise<void> {
  const { scopes, places, ui, transactions } = runtime();
  await scopes.ready;
  const browser = Services.wm.getMostRecentWindow("navigator:browser") as
    | Window
    | null;
  assert(browser, "a browser window owns the native transfer context");
  const before = scopes.getContext(browser);
  const a = crypto.randomUUID();
  const b = crypto.randomUUID();
  const suffix = crypto.randomUUID();
  const visibleURL = `https://transfer-visible-${suffix}.invalid/`;
  const hiddenURL = `https://transfer-hidden-${suffix}.invalid/`;
  const pref = "floorp.workspaces.bookmarks.enabled";
  const hadPref = Services.prefs.prefHasUserValue(pref);
  const oldPref = Services.prefs.getBoolPref(pref, true);
  const ruleGuids = new Set<string>();
  let rootGuid: string | undefined;
  let ownUndoEntries = 0;
  let createdOwnHistory = false;
  const view = { ownerWindow: browser, result: null };
  const folder = async (
    parentGuid: string,
    title: string,
  ): Promise<TransferBookmark> =>
    await places.bookmarks.insert({
      parentGuid,
      title,
      type: places.bookmarks.TYPE_FOLDER,
    });
  const item = (bookmark: TransferBookmark): TransferItem => ({
    itemGuid: bookmark.guid,
    parentGuid: bookmark.parentGuid,
    title: bookmark.title,
    type: places.TYPE_X_MOZ_PLACE_CONTAINER,
    instanceId: places.instanceId,
  });
  const point = (guid: string) => ({
    isTag: false as const,
    guid,
    getIndex: () => Promise.resolve(-1),
  });
  const scope = async (guid: string, ids: string[]): Promise<void> => {
    ruleGuids.add(guid);
    await scopes.setFolderScope(guid, ids);
  };
  const assertNoPendingCopy = async (): Promise<void> => {
    assert(
      scopes.getContext(browser).ready,
      "native transfer releases its in-memory scope gate",
    );
    const saved: unknown = await IOUtils.readJSON(
      PathUtils.join(
        PathUtils.profileDir,
        "workspaces",
        "bookmark-scopes.json",
      ),
    );
    assert(
      typeof saved === "object" && saved !== null,
      "scope rules are persisted on disk",
    );
    assertEquals(
      Reflect.get(saved, "pendingCopy"),
      undefined,
      "the native operation clears its durable copy marker",
    );
  };
  try {
    Services.prefs.setBoolPref(pref, true);
    const root = await folder(
      places.bookmarks.unfiledGuid,
      "Native transfer fixture",
    );
    rootGuid = root.guid;
    const source = await folder(root.guid, "Shared source");
    await places.bookmarks.insert({
      parentGuid: source.guid,
      title: "Eligible title",
      url: visibleURL,
    });
    const hidden = await folder(source.guid, "Nested B source");
    await places.bookmarks.insert({
      parentGuid: hidden.guid,
      title: "Hidden in A",
      url: hiddenURL,
    });
    const second = await folder(root.guid, "Second source");
    await places.bookmarks.insert({
      parentGuid: second.guid,
      title: "Second child",
      url: `https://transfer-second-${suffix}.invalid/`,
    });
    const target = await folder(root.guid, "Copy destination");
    const moveTarget = await folder(root.guid, "Narrower move destination");
    await scopes.refresh();
    await scope(source.guid, [a, b]);
    await scope(hidden.guid, [b]);
    await scope(second.guid, [a]);
    await scope(target.guid, [a, b]);
    await scope(moveTarget.guid, [a]);
    scopes.setWindowContext(browser, a, true);

    const selected = await ui.handleTransferItems(
      [item(source), item(second)],
      point(target.guid),
      true,
      view,
    );
    if (selected.length > 0) {
      ownUndoEntries++;
      createdOwnHistory = true;
    }
    assertEquals(
      selected.length,
      2,
      "two native Copy transactions return two flattened selected GUIDs",
    );
    assert(
      selected[0] !== source.guid && selected[1] !== second.guid,
      "native copies allocate fresh root GUIDs",
    );
    const copiedRoot = await places.bookmarks.fetch(selected[0]);
    const copiedSecond = await places.bookmarks.fetch(selected[1]);
    assert(
      copiedRoot && copiedSecond,
      "both returned GUIDs identify real Places folders",
    );
    assertEquals(
      copiedRoot.parentGuid,
      target.guid,
      "the first returned GUID corresponds to the first copied source",
    );
    assertEquals(
      copiedSecond.parentGuid,
      target.guid,
      "the second returned GUID corresponds to the second source",
    );
    const copiedTree = await places.promiseBookmarksTree(copiedRoot.guid);
    const copiedNested = copiedTree?.children?.[1];
    assert(
      copiedNested && copiedNested.typeCode === places.bookmarks.TYPE_FOLDER,
      "native Copy retains the known subtree position and type",
    );
    ruleGuids.add(copiedRoot.guid);
    ruleGuids.add(copiedSecond.guid);
    ruleGuids.add(copiedNested.guid);
    assertEquals(
      scopes.getScope(copiedRoot.guid)?.join(","),
      [a, b].sort().join(","),
      "root rules follow the native transaction's root GUID mapping",
    );
    assertEquals(
      scopes.getScope(copiedSecond.guid)?.join(","),
      a,
      "multiple-source copies keep each source's own rule",
    );
    assertEquals(
      scopes.getScope(copiedNested.guid)?.join(","),
      b,
      "nested explicit rules are assigned to real copied descendant GUIDs",
    );
    assertEquals(
      (await scopes.visibleBookmarksForUrl(visibleURL, browser)).length,
      2,
      "the original and copied eligible GUIDs both appear",
    );
    assertEquals(
      (await scopes.visibleBookmarksForUrl(hiddenURL, browser)).length,
      0,
      "copying a visible folder does not expose its hidden descendants",
    );
    await assertNoPendingCopy();

    const moved = await ui.handleTransferItems(
      [item(copiedRoot), item(copiedSecond)],
      point(moveTarget.guid),
      false,
      view,
    );
    if (moved.length > 0) ownUndoEntries++;
    assertEquals(
      moved.join(","),
      selected.join(","),
      "the native Move result preserves each selected GUID",
    );
    assertEquals(
      (await places.bookmarks.fetch(copiedRoot.guid))?.parentGuid,
      moveTarget.guid,
      "Move uses the actual requested destination",
    );
    scopes.setWindowContext(browser, b, true);
    assert(
      !scopes.isVisible(copiedRoot.guid, browser),
      "the moved root also inherits its new parent's narrower scope",
    );
    assert(
      scopes.isVisible(source.guid, browser),
      "moving copies leaves the original shared source intact",
    );
    await assertNoPendingCopy();

    await transactions.undo();
    ownUndoEntries--;
    await scopes.refresh();
    assertEquals(
      (await places.bookmarks.fetch(copiedRoot.guid))?.parentGuid,
      target.guid,
      "Undo Move restores the original parent",
    );
    assert(
      scopes.isVisible(copiedNested.guid, browser),
      "Undo restores the nested rule's original ancestor intersection",
    );
    await transactions.undo();
    ownUndoEntries--;
    await scopes.refresh();
    assertEquals(
      await places.bookmarks.fetch(copiedRoot.guid),
      null,
      "Undo Copy removes the native copied tree",
    );
    assertEquals(
      scopes.getScope(copiedNested.guid)?.join(","),
      b,
      "Undo keeps copied GUID rule tombstones for Redo",
    );
    await transactions.redo();
    ownUndoEntries++;
    await scopes.refresh();
    assert(
      await places.bookmarks.fetch(copiedRoot.guid),
      "Redo Copy restores the same root GUID",
    );
    assert(
      await places.bookmarks.fetch(copiedNested.guid),
      "Redo Copy restores the same descendant GUID",
    );
    assert(
      scopes.isVisible(copiedNested.guid, browser),
      "Redo reconnects retained rules without title or URL matching",
    );
    await transactions.redo();
    ownUndoEntries++;
    await scopes.refresh();
    assertEquals(
      (await places.bookmarks.fetch(copiedRoot.guid))?.parentGuid,
      moveTarget.guid,
      "Redo Move restores the real destination",
    );
    assert(
      !scopes.isVisible(copiedNested.guid, browser),
      "Redo Move reapplies the narrower ancestor scope",
    );
    await assertNoPendingCopy();
  } finally {
    try {
      for (const guid of ruleGuids) {
        if (await places.bookmarks.fetch(guid)) {
          await scopes.setFolderScope(guid, null);
        }
      }
      while (ownUndoEntries > 0) {
        await transactions.undo();
        ownUndoEntries--;
      }
      if (createdOwnHistory) {
        await transactions.clearTransactionsHistory(false, true);
      }
      if (rootGuid && await places.bookmarks.fetch(rootGuid)) {
        await places.bookmarks.remove(rootGuid);
      }
      await scopes.refresh();
    } finally {
      scopes.setWindowContext(browser, before.workspaceId, before.enabled);
      if (hadPref) Services.prefs.setBoolPref(pref, oldPref);
      else Services.prefs.clearUserPref(pref);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("WorkspaceBookmarkTransfers.test.mts", [
    {
      name: "real Places copy, move, undo and redo retain workspace scopes",
      fn: testRealTransferCopyMoveUndo,
    },
  ]);
}
