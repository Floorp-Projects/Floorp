// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { createDefaultNewTabSettings } from "#libs/newtab/settings.ts";
import type { NewTabSettingsPatch } from "#libs/newtab/types.ts";
import { createBookmarkService } from "../BookmarkService.sys.mts";
import {
  type BookmarkBackend,
  createMigrationRunner,
  type PlacesBookmark,
} from "../MigrationService.sys.mts";

class MemoryBookmarks implements BookmarkBackend {
  readonly TYPE_BOOKMARK = 1;
  readonly TYPE_FOLDER = 2;
  readonly rootGuid = "root________";
  readonly unfiledGuid = "unfiled_____";
  readonly userContentRoots = [this.unfiledGuid];
  readonly nodes = new Map<string, PlacesBookmark>();
  readonly listeners = new Set<() => void>();
  next = 0;
  inserts = 0;
  constructor() {
    this.nodes.set(this.rootGuid, {
      guid: this.rootGuid,
      parentGuid: "",
      type: 2,
      title: "Root",
      index: 0,
    });
    this.nodes.set(this.unfiledGuid, {
      guid: this.unfiledGuid,
      parentGuid: this.rootGuid,
      type: 2,
      title: "Other",
      index: 0,
    });
  }
  makeGuid() {
    return `test${String(++this.next).padStart(8, "0")}`;
  }
  emit() {
    for (const listener of this.listeners) listener();
  }
  childNodes(parent: string) {
    return [...this.nodes.values()].filter((node) => node.parentGuid === parent)
      .sort((a, b) => a.index - b.index);
  }
  fetch(
    query: string | { parentGuid: string },
    callback?: (node: PlacesBookmark) => void,
  ): Promise<PlacesBookmark | null> {
    const nodes = typeof query === "string"
      ? [this.nodes.get(query)].filter((node): node is PlacesBookmark => !!node)
      : this.childNodes(query.parentGuid);
    const output = nodes.map((node) => ({
      ...node,
      ...(node.type === 2
        ? { childCount: this.childNodes(node.guid).length }
        : {}),
    }));
    output.forEach((node) => callback?.(node));
    return Promise.resolve(output[0] ?? null);
  }
  insert(
    info: {
      guid?: string;
      parentGuid: string;
      type: number;
      title: string;
      url?: string;
      index?: number;
    },
  ): Promise<PlacesBookmark> {
    if (!this.nodes.has(info.parentGuid)) {
      return Promise.reject(new Error("Missing parent"));
    }
    const id = info.guid ?? this.makeGuid();
    if (this.nodes.has(id)) return Promise.reject(new Error("Duplicate GUID"));
    const node = {
      ...info,
      guid: id,
      index: info.index === undefined || info.index < 0
        ? this.childNodes(info.parentGuid).length
        : info.index,
    };
    this.nodes.set(id, node);
    this.inserts++;
    this.emit();
    return Promise.resolve({ ...node });
  }
  update(
    info: {
      guid: string;
      parentGuid?: string;
      title?: string;
      url?: string;
      index?: number;
    },
  ): Promise<PlacesBookmark> {
    const old = this.nodes.get(info.guid);
    if (!old) return Promise.reject(new Error("Missing node"));
    const index = info.index === -1
      ? this.childNodes(info.parentGuid ?? old.parentGuid).length
      : info.index ?? old.index;
    const node = { ...old, ...info, index };
    this.nodes.set(node.guid, node);
    this.emit();
    return Promise.resolve({ ...node });
  }
  async remove(id: string): Promise<void> {
    for (const child of this.childNodes(id)) await this.remove(child.guid);
    this.nodes.delete(id);
    this.emit();
  }
  reorder(parent: string, ids: string[]): Promise<void> {
    ids.forEach((id, index) => {
      const node = this.nodes.get(id);
      if (node?.parentGuid === parent) node.index = index;
    });
    this.emit();
    return Promise.resolve();
  }
}
function fixture() {
  const bookmarks = new MemoryBookmarks();
  let settings = createDefaultNewTabSettings();
  let journal: unknown | null = null;
  let writes = 0;
  let failAt = -1;
  const deps = {
    bookmarks,
    settings: {
      get: () => structuredClone(settings),
      updateInternal(changes: NewTabSettingsPatch) {
        settings = {
          ...settings,
          speedDial: { ...settings.speedDial, ...changes.speedDial },
          revision: settings.revision + 1,
        };
        return Promise.resolve(structuredClone(settings));
      },
    },
    storage: {
      read() {
        return Promise.resolve(structuredClone(journal));
      },
      write(value: unknown) {
        if (++writes === failAt) {
          return Promise.reject(
            new Error("Simulated interrupted journal write"),
          );
        }
        journal = structuredClone(value);
        return Promise.resolve();
      },
    },
    readRaw: () => "original-raw-prefs",
    makeGuid: () => bookmarks.makeGuid(),
    commonTitle: "Common",
  };
  const migration = createMigrationRunner(deps);
  const service = createBookmarkService({
    bookmarks,
    managedGuids: () => settings.speedDial,
    ensure: (options) => migration.ensure(options),
    observe(listener) {
      bookmarks.listeners.add(listener);
      return () => bookmarks.listeners.delete(listener);
    },
  });
  return {
    bookmarks,
    deps,
    migration,
    service,
    get settings() {
      return settings;
    },
    get journal() {
      return journal;
    },
    set failAt(value: number) {
      failAt = value;
    },
  };
}
function assertSequence(actual: string[], expected: string[], message: string) {
  assertEquals(JSON.stringify(actual), JSON.stringify(expected), message);
}
async function rejects(work: () => Promise<unknown>, code?: string) {
  let failed = false;
  try {
    await work();
  } catch (error) {
    failed = true;
    if (code) {
      assertEquals(
        (error as { code?: string }).code,
        code,
        "Failure has the expected code",
      );
    }
  }
  assert(failed, "The operation must fail without changing unrelated data");
}

async function testMigrationReceiptsAndRepair(): Promise<void> {
  const f = fixture();
  await Promise.all([f.service.ensure(), f.service.ensure()]);
  assertEquals(
    f.bookmarks.inserts,
    4,
    "One root, Common and every legacy pin (including the sponsor) are imported once",
  );
  const imported = await f.service.folder(f.settings.speedDial.commonGuid!);
  assertSequence(
    imported.children.map((item) => item.title),
    f.settings.topSites.pinned.map((pin) => pin.title),
    "Imported names and order match every saved pin",
  );
  assertEquals(
    (f.journal as { originalRaw: string }).originalRaw,
    "original-raw-prefs",
    "The raw source is retained before any Places writes",
  );
  await f.service.ensure();
  assertEquals(
    f.bookmarks.inserts,
    4,
    "Repeated ensure does not reimport pins",
  );
  const root = f.settings.speedDial.rootGuid!;
  const common = f.settings.speedDial.commonGuid!;
  await f.bookmarks.remove(common);
  await rejects(() => f.service.ensure(), "UNAVAILABLE");
  assertEquals(
    f.bookmarks.inserts,
    4,
    "A deleted Common folder is not resurrected automatically",
  );
  await f.service.ensure({ repair: true });
  assertEquals(
    f.settings.speedDial.rootGuid,
    root,
    "Repair preserves the existing root",
  );
  assert(
    f.settings.speedDial.commonGuid !== common,
    "Repair allocates a new Common identity",
  );
  assertEquals(
    (await f.service.folder(f.settings.speedDial.commonGuid!)).children.length,
    0,
    "Explicit repair makes an empty entry and does not import old pins again",
  );
  f.service.shutdown();
}
async function testInterruptedImportAndUserEdits(): Promise<void> {
  const f = fixture();
  f.settings.topSites.pinned[0].url = "https://example.test";
  f.failAt = 4; // First pin committed, but its durable receipt is still planned.
  await rejects(() => f.migration.ensure());
  const restarted = createMigrationRunner(f.deps);
  await rejects(() => restarted.ensure(), "UNAVAILABLE");
  await restarted.ensure({ repair: true });
  assertEquals(
    f.bookmarks.inserts,
    4,
    "Matching committed GUIDs are reused after an interrupted write",
  );
  const common = f.settings.speedDial.commonGuid!;
  const item = f.bookmarks.childNodes(common)[0];
  await f.bookmarks.update({ guid: item.guid, title: "User edited title" });
  await restarted.ensure();
  assertEquals(
    f.bookmarks.nodes.get(item.guid)?.title,
    "User edited title",
    "Ensure preserves edits made after migration",
  );
  await f.bookmarks.remove(item.guid);
  await restarted.ensure();
  assert(
    !f.bookmarks.nodes.has(item.guid),
    "A completed import never recreates a user-deleted item",
  );
}
async function testBookmarkScopeAndNestedMoves(): Promise<void> {
  const f = fixture();
  await f.service.ensure();
  const common = f.settings.speedDial.commonGuid!;
  const a = (await f.service.mutate({
    action: "create",
    type: "folder",
    parentGuid: common,
    title: "Parent",
  })).guid!;
  const b = (await f.service.mutate({
    action: "create",
    type: "folder",
    parentGuid: a,
    title: "Child",
  })).guid!;
  const site = (await f.service.mutate({
    action: "create",
    type: "bookmark",
    parentGuid: b,
    title: "Documentation",
    url: "https://example.test/docs",
  })).guid!;
  await rejects(
    () => f.service.mutate({ action: "move", guid: a, parentGuid: b }),
    "VALIDATION",
  );
  await rejects(
    () => f.service.mutate({ action: "move", guid: a, parentGuid: a }),
    "VALIDATION",
  );
  await rejects(() => f.service.folder(f.bookmarks.unfiledGuid), "VALIDATION");
  await rejects(
    () =>
      f.service.mutate({
        action: "create",
        type: "bookmark",
        parentGuid: common,
        title: "Unsafe",
        url: "javascript:alert(1)",
      }),
    "VALIDATION",
  );
  const other = (await f.service.mutate({
    action: "create",
    type: "folder",
    parentGuid: f.settings.speedDial.rootGuid!,
    title: "Other group",
  })).guid!;
  await f.service.mutate({ action: "move", guid: a, parentGuid: other });
  assertSequence(
    (await f.service.folder(b)).ancestors.map((node) => node.guid),
    [
      other,
      a,
      b,
    ],
    "The subtree is located under its new group",
  );
  assertEquals(
    f.bookmarks.nodes.get(site)?.parentGuid,
    b,
    "Moving a folder preserves its descendants' GUIDs",
  );
  const summary = await f.service.deleteSummary(a);
  assertEquals(summary.folders, 1, "Deletion counts nested folders");
  assertEquals(summary.bookmarks, 1, "Deletion counts nested bookmarks");
  await f.bookmarks.update({ guid: site, title: "Concurrent edit" });
  await rejects(
    () =>
      f.service.mutate({
        action: "delete",
        guid: a,
        expectedRevision: summary.revision,
      }),
    "CONFLICT",
  );
  const updated = await f.service.deleteSummary(a);
  await f.service.mutate({
    action: "delete",
    guid: a,
    expectedRevision: updated.revision,
  });
  assert(
    !f.bookmarks.nodes.has(site),
    "Confirmed deletion removes descendants",
  );
  assert(f.bookmarks.nodes.has(other), "The containing group is preserved");
  f.service.shutdown();
}
async function testMixedOrderAndUnsupportedNodes(): Promise<void> {
  const f = fixture();
  await f.service.ensure();
  const common = f.settings.speedDial.commonGuid!;
  const before = await f.service.folder(common);
  const folder = (await f.service.mutate({
    action: "create",
    type: "folder",
    parentGuid: common,
    title: "Folder",
  })).guid!;
  const separator = await f.bookmarks.insert({
    parentGuid: common,
    type: 3,
    title: "separator",
  });
  const query = await f.bookmarks.insert({
    parentGuid: common,
    type: 1,
    title: "Query",
    url: "place:sort=8",
  });
  const view = await f.service.folder(common);
  assert(
    !view.children.some((node) =>
      node.guid === separator.guid || node.guid === query.guid
    ),
    "Unsupported nodes are excluded from the interactive list",
  );
  const ids = [folder, ...before.children.map((node) => node.guid)];
  await f.service.mutate({ action: "reorder", parentGuid: common, guids: ids });
  assertSequence(
    (await f.service.folder(common)).children.map((node) => node.guid),
    ids,
    "Folders and bookmarks reorder together",
  );
  assert(
    f.bookmarks.nodes.has(separator.guid) && f.bookmarks.nodes.has(query.guid),
    "Unsupported Places nodes are not deleted",
  );
  await rejects(
    () =>
      f.service.mutate({
        action: "reorder",
        parentGuid: common,
        guids: [folder],
      }),
    "CONFLICT",
  );
  f.service.shutdown();
}
async function testSourceConflictAndInvalidJournal(): Promise<void> {
  const f = fixture();
  f.failAt = 4;
  await rejects(() => f.migration.ensure());
  const inserted = f.bookmarks.inserts;
  f.settings.topSites.pinned.push({
    url: "https://new.example.test/",
    title: "Added in another window",
  });
  await rejects(
    () => createMigrationRunner(f.deps).ensure({ repair: true }),
    "CONFLICT",
  );
  assertEquals(
    f.bookmarks.inserts,
    inserted,
    "Changed source data pauses import without overwriting either copy",
  );
  const untouched = fixture();
  let writes = 0;
  const invalid = createMigrationRunner({
    ...untouched.deps,
    storage: {
      read: () => Promise.resolve({ version: 99 }),
      write: () => {
        writes++;
        return Promise.resolve();
      },
    },
  });
  await rejects(() => invalid.ensure(), "UNAVAILABLE");
  assertEquals(writes, 0, "An unknown journal version is not overwritten");
  assertEquals(
    untouched.bookmarks.inserts,
    0,
    "An invalid journal does not create any folders",
  );
}
async function testNativePlacesOperations(): Promise<void> {
  const { PlacesUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesUtils.sys.mjs",
  ) as { PlacesUtils: { bookmarks: BookmarkBackend } };
  const backend = PlacesUtils.bookmarks;
  const root = await backend.insert({
    parentGuid: backend.unfiledGuid,
    type: backend.TYPE_FOLDER,
    title: "Floorp New Tab test temporary root",
  });
  const common = await backend.insert({
    parentGuid: root.guid,
    type: backend.TYPE_FOLDER,
    title: "Common",
  });
  const service = createBookmarkService({
    bookmarks: backend,
    managedGuids: () => ({ rootGuid: root.guid, commonGuid: common.guid }),
    ensure: async () => {},
    observe: () => () => {},
  });
  try {
    const nested = (await service.mutate({
      action: "create",
      type: "folder",
      parentGuid: common.guid,
      title: "Nested",
    })).guid!;
    const bookmark = (await service.mutate({
      action: "create",
      type: "bookmark",
      parentGuid: nested,
      title: "Native child",
      url: "https://example.test/native",
    })).guid!;
    assertEquals(
      (await service.folder(nested)).children[0].guid,
      bookmark,
      "Native Places fetch returns the new child",
    );
    await rejects(
      () =>
        service.mutate({ action: "move", guid: nested, parentGuid: nested }),
      "VALIDATION",
    );
    await service.mutate({
      action: "move",
      guid: bookmark,
      parentGuid: common.guid,
    });
    assertEquals(
      (await backend.fetch(bookmark))?.parentGuid,
      common.guid,
      "Native Places move retains the bookmark identity",
    );
  } finally {
    service.shutdown();
    await backend.remove(root.guid);
  }
}
export async function runAllTests(): Promise<void> {
  await runTests("BookmarkService.test.mts", [
    {
      name: "legacy import receipts, deduplication and empty repair",
      fn: testMigrationReceiptsAndRepair,
    },
    {
      name:
        "interrupted imports reuse committed identities and respect user changes",
      fn: testInterruptedImportAndUserEdits,
    },
    {
      name: "nested moves enforce scope, cycles and deletion revisions",
      fn: testBookmarkScopeAndNestedMoves,
    },
    {
      name: "mixed ordering preserves separators and unsupported query nodes",
      fn: testMixedOrderAndUnsupportedNodes,
    },
    {
      name:
        "source changes and unknown journal versions fail non-destructively",
      fn: testSourceConflictAndInvalidJournal,
    },
    {
      name: "native Places stores nested moves without copied identities",
      fn: testNativePlacesOperations,
    },
  ]);
}
