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
  Services.prefs.clearUserPref("nora.tests.bookmark_palette_timeout");
  const timers = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  ) as {
    setTimeout(callback: () => void, delay: number): number;
    clearTimeout(id: number): void;
  };
  // Use a module timer so a backgrounded chrome window cannot suppress the
  // diagnostic deadline. Keep the last operation in the collected test prefs.
  const step = async <T>(name: string, operation: () => Promise<T>) => {
    Services.prefs.setStringPref("nora.tests.bookmark_palette_stage", name);
    Services.prefs.savePrefFile(null as unknown as nsIFile);
    let deadline = 0;
    try {
      return await Promise.race([
        new Promise<never>((_resolve, reject) => {
          deadline = timers.setTimeout(() => {
            if (
              !Services.prefs.prefHasUserValue(
                "nora.tests.bookmark_palette_timeout",
              )
            ) {
              Services.prefs.setStringPref(
                "nora.tests.bookmark_palette_timeout",
                name,
              );
            }
            Services.prefs.savePrefFile(null as unknown as nsIFile);
            reject(
              new Error(
                `Bookmark palette ${name} timed out: ${
                  JSON.stringify(scopes.getStatus())
                }`,
              ),
            );
          }, 30_000);
        }),
        Promise.resolve().then(operation),
      ]);
    } finally {
      timers.clearTimeout(deadline);
    }
  };
  await step("initial scope readiness", () => scopes.ready);
  const { PlacesUtils: places } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: PlacesFixture };
  const prefix = `PaletteScope${crypto.randomUUID().replaceAll("-", "")}`;
  const A = crypto.randomUUID();
  const B = crypto.randomUUID();
  const scopeEnabled = scopes.getEnabled();
  // Visibility resolves the owner's native privacy context on every lookup.
  // Keep a separate window identity for the navigation spy, while supplying
  // the real chrome docshell instead of making PrivateBrowsingUtils warn and
  // dump a stack for every row in the tree.
  const target = {
    closed: false,
    isChromeWindow: true,
    docShell: window.docShell,
  } as Window;
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
    const root = await step(
      "create fixture root",
      () =>
        places.bookmarks.insert({
          parentGuid: places.bookmarks.unfiledGuid,
          title: prefix,
          type: places.bookmarks.TYPE_FOLDER,
        }),
    );
    rootGuid = root.guid;
    const hidden = await step(
      "create hidden folder",
      () =>
        places.bookmarks.insert({
          parentGuid: root.guid,
          title: `${prefix} hidden`,
          type: places.bookmarks.TYPE_FOLDER,
        }),
    );
    hiddenGuid = hidden.guid;
    const visible = await step(
      "create visible folder",
      () =>
        places.bookmarks.insert({
          parentGuid: root.guid,
          title: `${prefix} visible`,
          type: places.bookmarks.TYPE_FOLDER,
        }),
    );
    visibleGuid = visible.guid;
    const sharedURL = `https://example.com/${prefix}/shared`;
    await step("create hidden duplicate", () =>
      places.bookmarks.insert({
        parentGuid: hidden.guid,
        title: `${prefix} HiddenTitle`,
        url: sharedURL,
      }));
    await step("create visible duplicate", () =>
      places.bookmarks.insert({
        parentGuid: visible.guid,
        title: `${prefix} VisibleTitle`,
        url: sharedURL,
      }));
    // Enough results to force deferred pagination. Hidden rows precede visible
    // rows in Places so filtering after limiting or URL deduplication is caught.
    for (let index = 0; index < 55; index++) {
      await step(
        `create pagination bookmark ${index}`,
        () =>
          places.bookmarks.insert({
            parentGuid: visible.guid,
            title: `${prefix} Visible ${index}`,
            url: `https://example.com/${prefix}/${index}`,
          }),
      );
    }
    await step(
      "persist hidden scope",
      () => scopes.setFolderScope(hidden.guid, [B]),
    );
    await step(
      "persist visible scope",
      () => scopes.setFolderScope(visible.guid, [A]),
    );
    await step("refresh fixture index", () => scopes.refresh());
    const commands = await step(
      "search scoped bookmarks",
      () => searchBookmarks(prefix, 100, target),
    );
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
      (await step(
        "search with result limit",
        () => searchBookmarks(prefix, 1, target),
      )).length,
      1,
      "limit applies after visibility",
    );
    const snapshot = await step(
      "load switcher snapshot",
      () => loadBookmarks(target),
    );
    assert(!Array.isArray(snapshot), "bookmark switcher supplies a snapshot");
    assert(snapshot.hasMore && snapshot.loadMore, "fixture has deferred pages");
    const loadMore = snapshot.loadMore;
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
    const stale = await step("reject stale pagination", loadMore);
    assertEquals(
      stale.choices.length,
      0,
      "A to B to A invalidates deferred pages",
    );
    assertEquals(stale.hasMore, false, "stale pages cannot continue");
    scopes.setWindowContext(target, B, true);
    const other = await step(
      "search other workspace",
      () => searchBookmarks(prefix, 100, target),
    );
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
    try {
      if (visibleGuid) {
        const guid = visibleGuid;
        await step(
          "clean visible scope",
          () => scopes.setFolderScope(guid, null),
        );
      }
      if (hiddenGuid) {
        const guid = hiddenGuid;
        await step(
          "clean hidden scope",
          () => scopes.setFolderScope(guid, null),
        );
      }
      if (rootGuid) {
        const guid = rootGuid;
        await step(
          "remove fixture tree",
          () => places.bookmarks.remove(guid),
        );
      }
      await step("refresh cleaned index", () => scopes.refresh());
    } finally {
      scopes.clearWindowContext(target);
      scopes.setEnabled(scopeEnabled);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("paletteScopes.test.ts", [{
    name:
      "real Places palette filters before deduplication and pagination, then rejects stale actions",
    fn: testScopedPaletteWithRealPlaces,
  }]);
}
