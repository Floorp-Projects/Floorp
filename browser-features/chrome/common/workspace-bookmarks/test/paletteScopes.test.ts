// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { getWorkspaceBookmarks } from "../service.ts";
import { searchBookmarks } from "../../command-palette/bookmark-provider.ts";
import { loadBookmarks } from "../../command-palette/multiInputCommand/switcher/bookmark-switcher.ts";
import type { PlacesFixture } from "#modules/modules/test/WorkspaceBookmarkRuntime.types.ts";

async function testScopedPaletteWithRealPlaces(): Promise<void> {
  const scopes = getWorkspaceBookmarks();
  await scopes.ready;
  const { PlacesUtils: places } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: PlacesFixture };
  const prefix = `PaletteScope${crypto.randomUUID().replaceAll("-", "")}`;
  const A = crypto.randomUUID();
  const B = crypto.randomUUID();
  const scopeEnabled = scopes.getEnabled();
  const target = { closed: false } as Window;
  let rootGuid: string | undefined;
  let hiddenGuid: string | undefined;
  let visibleGuid: string | undefined;
  let navigations = 0;
  Reflect.set(target, "gBrowser", {
    loadURI: () => {
      navigations++;
    },
  });
  try {
    scopes.setEnabled(true);
    scopes.setWindowContext(target, A, true);
    const root = await places.bookmarks.insert({
      parentGuid: places.bookmarks.unfiledGuid,
      title: prefix,
      type: places.bookmarks.TYPE_FOLDER,
    });
    rootGuid = root.guid;
    const hidden = await places.bookmarks.insert({
      parentGuid: root.guid,
      title: `${prefix} hidden`,
      type: places.bookmarks.TYPE_FOLDER,
    });
    hiddenGuid = hidden.guid;
    const visible = await places.bookmarks.insert({
      parentGuid: root.guid,
      title: `${prefix} visible`,
      type: places.bookmarks.TYPE_FOLDER,
    });
    visibleGuid = visible.guid;
    const sharedURL = `https://example.com/${prefix}/shared`;
    await places.bookmarks.insert({
      parentGuid: hidden.guid,
      title: `${prefix} HiddenTitle`,
      url: sharedURL,
    });
    await places.bookmarks.insert({
      parentGuid: visible.guid,
      title: `${prefix} VisibleTitle`,
      url: sharedURL,
    });
    // Enough results to force deferred pagination. Hidden rows precede visible
    // rows in Places so filtering after limiting or URL deduplication is caught.
    for (let index = 0; index < 55; index++) {
      await places.bookmarks.insert({
        parentGuid: visible.guid,
        title: `${prefix} Visible ${index}`,
        url: `https://example.com/${prefix}/${index}`,
      });
    }
    await scopes.setFolderScope(hidden.guid, [B]);
    await scopes.setFolderScope(visible.guid, [A]);
    await scopes.refresh();
    const commands = await searchBookmarks(prefix, 100, target);
    assertEquals(
      commands.length,
      56,
      "all visible records survive hidden duplicates",
    );
    assert(
      commands.every((item) => !item.label.includes("HiddenTitle")),
      "hidden title is absent",
    );
    assert(
      commands.some((item) => item.label.includes("VisibleTitle")),
      "same URL uses visible GUID metadata",
    );
    assertEquals(
      (await searchBookmarks(prefix, 1, target)).length,
      1,
      "limit applies after visibility",
    );
    const snapshot = await loadBookmarks(target);
    assert(!Array.isArray(snapshot), "bookmark switcher supplies a snapshot");
    assert(snapshot.hasMore && snapshot.loadMore, "fixture has deferred pages");
    assert(
      snapshot.choices.every((item) => !item.label.includes("HiddenTitle")),
      "first page is scoped",
    );
    scopes.setWindowContext(target, B, true);
    scopes.setWindowContext(target, A, true);
    await commands[0].fn(target);
    assertEquals(
      navigations,
      0,
      "A to B to A invalidates old navigation closures",
    );
    const stale = await snapshot.loadMore();
    assertEquals(
      stale.choices.length,
      0,
      "A to B to A invalidates deferred pages",
    );
    assertEquals(stale.hasMore, false, "stale pages cannot continue");
    scopes.setWindowContext(target, B, true);
    const other = await searchBookmarks(prefix, 100, target);
    assertEquals(
      other.length,
      1,
      "other window context has only its own same-URL copy",
    );
    assert(
      other[0].label.includes("HiddenTitle"),
      "other context resolves its own metadata",
    );
  } finally {
    if (visibleGuid) await scopes.setFolderScope(visibleGuid, null);
    if (hiddenGuid) await scopes.setFolderScope(hiddenGuid, null);
    if (rootGuid) await places.bookmarks.remove(rootGuid);
    await scopes.refresh();
    scopes.setEnabled(scopeEnabled);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("paletteScopes.test.ts", [{
    name:
      "real Places palette filters before deduplication and pagination, then rejects stale actions",
    fn: testScopedPaletteWithRealPlaces,
  }]);
}
