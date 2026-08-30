// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuCatalogSnapshot,
  type ContextMenuSurfaceDescriptor,
} from "../../../chrome/common/context-menu/types.ts";
import { ContextMenuCatalogStore } from "./ContextMenuCatalogService.sys.mts";

function assertJsonEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  assertEquals(JSON.stringify(actual), JSON.stringify(expected), message);
}

function makeSurface(
  key: string,
  label: string,
): ContextMenuSurfaceDescriptor {
  return {
    key,
    label,
    profiles: [{
      key: "default",
      label: "Default",
      containers: [{
        key: "root",
        label: "Root",
        complete: true,
        items: [{
          key: `${key}.item`,
          label: `${label} item`,
          kind: "command",
          source: "firefox",
          customizable: true,
          nativeHidden: false,
        }],
      }],
    }],
  };
}

function makeSnapshot(
  locale: string,
  surfaces: ContextMenuSurfaceDescriptor[],
  revision = 1,
): ContextMenuCatalogSnapshot {
  return {
    schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
    revision,
    locale,
    surfaces,
  };
}

function surfaceLabel(
  snapshot: ContextMenuCatalogSnapshot,
  key: string,
): string | undefined {
  return snapshot.surfaces.find((surface) => surface.key === key)?.label;
}

function testEmptySnapshot(): void {
  const store = new ContextMenuCatalogStore();
  assertJsonEquals(store.getSnapshot(), {
    schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
    revision: 0,
    locale: "",
    surfaces: [],
  }, "a store with no owners returns the serializable empty snapshot");
}

function testLatestOwnerWinsPerSurface(): void {
  const store = new ContextMenuCatalogStore();
  store.report(
    "window-a",
    makeSnapshot("en-US", [
      makeSurface("browser.content", "Content A"),
      makeSurface("browser.tabs", "Tabs A"),
    ]),
  );
  store.report(
    "window-b",
    makeSnapshot("ja", [
      makeSurface("browser.content", "Content B"),
      makeSurface("library.places", "Places B"),
    ]),
  );

  const aggregate = store.getSnapshot();
  assertEquals(aggregate.revision, 2, "each owner report advances revision");
  assertEquals(aggregate.locale, "ja", "the latest owner supplies locale");
  assertJsonEquals(
    aggregate.surfaces.map((surface) => surface.key),
    ["browser.content", "browser.tabs", "library.places"],
    "the service returns the stable union sorted by surface key",
  );
  assertEquals(
    surfaceLabel(aggregate, "browser.content"),
    "Content B",
    "the latest report wins when owners report the same surface",
  );
  assertEquals(
    surfaceLabel(aggregate, "browser.tabs"),
    "Tabs A",
    "an older owner's unique surface remains in the union",
  );
}

function testOwnerReplacementAndRemoval(): void {
  const store = new ContextMenuCatalogStore();
  store.report(
    "window-a",
    makeSnapshot("en-US", [makeSurface("browser.content", "Content A")]),
  );
  store.report(
    "window-b",
    makeSnapshot("ja", [makeSurface("browser.content", "Content B")]),
  );
  store.report(
    "window-a",
    makeSnapshot("fr", [makeSurface("browser.content", "Content A2")], 2),
  );

  assertEquals(
    surfaceLabel(store.getSnapshot(), "browser.content"),
    "Content A2",
    "a replacement report makes that owner the latest surface provider",
  );

  store.removeOwner("window-a");
  const fallback = store.getSnapshot();
  assertEquals(
    surfaceLabel(fallback, "browser.content"),
    "Content B",
    "removing the latest owner reveals the next newest provider",
  );
  assertEquals(
    fallback.locale,
    "ja",
    "locale follows the latest remaining owner",
  );
  assertEquals(
    fallback.revision,
    4,
    "removing an existing owner advances revision",
  );

  store.removeOwner("missing-owner");
  assertEquals(
    store.getSnapshot().revision,
    4,
    "removing an unknown owner does not produce a false revision",
  );

  store.removeOwner("window-b");
  const empty = store.getSnapshot();
  assertEquals(empty.locale, "", "an empty aggregate has no locale");
  assertJsonEquals(
    empty.surfaces,
    [],
    "removing all owners empties the catalog",
  );
  assertEquals(empty.revision, 5, "the empty transition is observable");
}

function testIncompleteOwnerDoesNotEraseCompleteContainer(): void {
  const store = new ContextMenuCatalogStore();
  const complete = makeSurface("browser.content", "Content A");
  const placeholder = makeSurface("browser.content", "Content B");
  placeholder.profiles[0].containers[0] = {
    key: "root",
    label: "Root",
    complete: false,
    items: [],
  };

  store.report("window-a", makeSnapshot("en-US", [complete]));
  store.report("window-b", makeSnapshot("ja", [placeholder]));

  const aggregate = store.getSnapshot();
  assertEquals(
    surfaceLabel(aggregate, "browser.content"),
    "Content B",
    "the latest owner may refresh surface metadata",
  );
  const root = aggregate.surfaces[0].profiles[0].containers[0];
  assert(root.complete, "an older complete container remains complete");
  assertEquals(
    root.items[0].key,
    "browser.content.item",
    "a new window's placeholder does not erase captured items",
  );
}

function testEmptyIncompleteOwnerDoesNotEraseSeededContainer(): void {
  const store = new ContextMenuCatalogStore();
  const seeded = makeSurface("browser.content", "Seeded");
  seeded.profiles[0].containers[0].complete = false;
  seeded.profiles[0].containers[0].items[0].label = "Seeded item";
  const placeholder = makeSurface("browser.content", "Placeholder");
  placeholder.profiles[0].containers[0] = {
    key: "root",
    label: "Root",
    complete: false,
    items: [],
  };

  store.report("main-window", makeSnapshot("en-US", [seeded]));
  store.report("secondary-window", makeSnapshot("en-US", [placeholder]));

  let root = store.getSnapshot().surfaces[0].profiles[0].containers[0];
  assertEquals(
    root.items[0]?.label,
    "Seeded item",
    "a later empty placeholder cannot erase useful provisional rows",
  );
  assertEquals(root.complete, false, "the retained DOM seed stays provisional");

  const newerSeed = makeSurface("browser.content", "Newer seed");
  newerSeed.profiles[0].containers[0].complete = false;
  newerSeed.profiles[0].containers[0].items[0].label = "Newer seeded item";
  store.report("secondary-window", makeSnapshot("en-US", [newerSeed], 2));
  root = store.getSnapshot().surfaces[0].profiles[0].containers[0];
  assertEquals(
    root.items[0]?.label,
    "Newer seeded item",
    "a later populated provisional report can refresh an older seed",
  );
}

function testUnchangedContainersAreNotReagedByUnrelatedReports(): void {
  const store = new ContextMenuCatalogStore();
  const contentA = makeSurface("browser.content", "Content");
  contentA.profiles[0].containers[0].items[0].label = "Content from A";
  const contentB = makeSurface("browser.content", "Content");
  contentB.profiles[0].containers[0].items[0].label = "Content from B";
  const tabsA1 = makeSurface("browser.tabs", "Tabs");
  tabsA1.profiles[0].containers[0].items[0].label = "Tabs A1";
  const tabsA2 = makeSurface("browser.tabs", "Tabs");
  tabsA2.profiles[0].containers[0].items[0].label = "Tabs A2";

  store.report("window-a", makeSnapshot("en-US", [contentA, tabsA1]));
  store.report("window-b", makeSnapshot("en-US", [contentB]));
  store.report("window-a", makeSnapshot("en-US", [contentA, tabsA2], 2));

  const aggregate = store.getSnapshot();
  const contentItem = aggregate.surfaces.find((surface) =>
    surface.key === "browser.content"
  )?.profiles[0].containers[0].items[0];
  const tabsItem = aggregate.surfaces.find((surface) =>
    surface.key === "browser.tabs"
  )?.profiles[0].containers[0].items[0];
  assertEquals(
    contentItem?.label,
    "Content from B",
    "an unrelated update must not make window A's stale content catalog newest",
  );
  assertEquals(
    tabsItem?.label,
    "Tabs A2",
    "the container that actually changed still becomes newest",
  );

  store.removeOwner("window-b");
  const fallbackContent = store.getSnapshot().surfaces.find((surface) =>
    surface.key === "browser.content"
  )?.profiles[0].containers[0].items[0];
  assertEquals(
    fallbackContent?.label,
    "Content from A",
    "removing the newest observer reveals the remaining owner's catalog",
  );
}

function testSnapshotsAreDetachedAndSerializable(): void {
  const store = new ContextMenuCatalogStore();
  const input = makeSnapshot("en-US", [
    makeSurface("browser.content", "Original"),
  ]);
  store.report("window-a", input);

  input.surfaces[0].label = "Mutated input";
  const first = store.getSnapshot();
  assertEquals(
    surfaceLabel(first, "browser.content"),
    "Original",
    "report stores a detached snapshot",
  );

  first.surfaces[0].profiles[0].containers[0].items[0].label = "Mutated output";
  assertEquals(
    store.getSnapshot().surfaces[0].profiles[0].containers[0].items[0].label,
    "Original item",
    "getSnapshot returns a detached graph on every call",
  );

  const serialized = JSON.stringify(store.getSnapshot());
  const parsed = JSON.parse(serialized) as Record<string, unknown>;
  assert(
    Array.isArray(parsed.surfaces),
    "the aggregate survives JSON transport",
  );
}

function testRejectsEmptyOwnerId(): void {
  const store = new ContextMenuCatalogStore();
  let threw = false;
  try {
    store.report("", makeSnapshot("en-US", []));
  } catch (error) {
    threw = error instanceof TypeError;
  }
  assert(threw, "an empty owner id must not silently share global state");
  assertEquals(
    store.getSnapshot().revision,
    0,
    "a rejected report has no effect",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "empty snapshot", fn: testEmptySnapshot },
    {
      name: "latest owner wins per surface",
      fn: testLatestOwnerWinsPerSurface,
    },
    {
      name: "owner replacement and removal",
      fn: testOwnerReplacementAndRemoval,
    },
    {
      name: "incomplete owner preserves complete containers",
      fn: testIncompleteOwnerDoesNotEraseCompleteContainer,
    },
    {
      name: "empty incomplete owner preserves seeded containers",
      fn: testEmptyIncompleteOwnerDoesNotEraseSeededContainer,
    },
    {
      name: "unrelated owner reports do not re-age unchanged containers",
      fn: testUnchangedContainersAreNotReagedByUnrelatedReports,
    },
    {
      name: "snapshots are detached and serializable",
      fn: testSnapshotsAreDetachedAndSerializable,
    },
    { name: "empty owner id is rejected", fn: testRejectsEmptyOwnerId },
  ];
  await runTests("ContextMenuCatalogService.test.mts", tests);
}
