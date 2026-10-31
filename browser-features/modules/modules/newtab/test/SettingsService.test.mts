// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  createDefaultNewTabSettings,
  mergeNewTabSettings,
  normalizeNewTabSettings,
} from "../../../../../libs/newtab/settings.ts";
import { NewTabSettingsStore } from "../SettingsService.sys.mts";
import type { NewTabSettingsPatch } from "../../../../../libs/newtab/types.ts";

function fakeStore(initial: string | null = null) {
  let raw = initial;
  let writes = 0;
  let fail = false;
  const observers = new Set<() => void>();
  const store = new NewTabSettingsStore({
    read: () => raw,
    write(value) {
      if (fail) throw new Error("Disk unavailable");
      raw = value;
      writes++;
      for (const observer of observers) observer();
    },
    observe(listener) {
      observers.add(listener);
      return () => observers.delete(listener);
    },
  });
  return {
    store,
    raw: () => raw,
    writes: () => writes,
    failWrites: () => {
      fail = true;
    },
    externalWrite(value: string) {
      raw = value;
      for (const observer of observers) observer();
    },
  };
}

function testLegacyAndUnknownFields(): void {
  const raw = {
    customFutureKey: { keep: "yes" },
    components: { topSites: false, clock: false, customLegacy: "keep" },
    background: {
      type: "folderPath",
      folderPath: "/Pictures/Wallpapers",
      fileName: "custom.jpg",
      customImage: "data:image/jpeg;base64,abc",
      selectedFloorp: "night.png",
      slideshowEnabled: true,
      slideshowInterval: 17,
      futureEffect: 7,
    },
    topSites: {
      pinned: [{ title: "My sponsor link", url: "https://www.cube-soft.jp/" }, {
        title: "Duplicate URL kept",
        url: "https://www.cube-soft.jp/",
      }],
      blocked: ["https://example.com/"],
    },
    searchBar: { searchEngine: "existing-engine" },
  };
  const settings = normalizeNewTabSettings(raw);
  assertEquals(
    settings.speedDial.show,
    false,
    "Legacy top sites visibility maps to dials",
  );
  assertEquals(
    settings.home.showHistory,
    false,
    "Legacy top sites visibility maps to history",
  );
  assertEquals(
    settings.background.folderPath,
    raw.background.folderPath,
    "Folder selection survives",
  );
  assertEquals(
    settings.background.slideshowInterval,
    17,
    "Existing seconds survive",
  );
  assertEquals(
    settings.topSites.pinned.length,
    2,
    "No URL based pin deduplication",
  );
  assertEquals(
    settings.searchBar.searchEngine,
    "existing-engine",
    "Stored search engine survives",
  );
  for (const interval of [0, 1.5, 604800]) {
    const old = normalizeNewTabSettings({
      background: { slideshowInterval: interval },
    });
    assertEquals(
      old.background.slideshowInterval,
      interval,
      "Legacy intervals are retained without truncation",
    );
    assertEquals(
      mergeNewTabSettings(old, { home: { showFeed: false } }).background
        .slideshowInterval,
      interval,
      "An unrelated patch preserves legacy timing",
    );
  }
  const changed = mergeNewTabSettings(settings, { home: { showFeed: false } });
  const saved = JSON.parse(JSON.stringify(changed)) as {
    customFutureKey: { keep: string };
    background: { futureEffect: number };
    components: { customLegacy: string };
  };
  assertEquals(
    saved.customFutureKey.keep,
    "yes",
    "Unknown root setting survives",
  );
  assertEquals(
    saved.background.futureEffect,
    7,
    "Unknown nested setting survives",
  );
  assertEquals(
    saved.components.customLegacy,
    "keep",
    "Unknown legacy setting survives",
  );
}

function testInvalidDataIsNotReplaced(): void {
  for (
    const raw of [
      "broken-json",
      "null",
      "[]",
      '{"schemaVersion":999}',
      '{"components":{"clock":"yes"}}',
    ]
  ) {
    const fake = fakeStore(raw);
    assertThrows(() => fake.store.get(), "Invalid data is rejected");
    assertEquals(fake.raw(), raw, "Read never repairs the original preference");
    assertEquals(fake.writes(), 0, "Read performs no write");
  }
}

function testPatchBoundary(): void {
  const settings = createDefaultNewTabSettings();
  for (
    const patch of [
      { schemaVersion: 999 },
      { revision: 99 },
      { unrelated: true },
      { speedDial: { rootGuid: "foreignRoot_" } },
      { topSites: { pinned: [] } },
      { background: { type: "remote" } },
      { background: { slideshowInterval: 0 } },
      { speedDial: { columns: 1000 } },
      { rss: { refreshMinutes: -1 } },
      { home: { showFeed: "false" } },
    ]
  ) {
    assertThrows(
      () => mergeNewTabSettings(settings, patch as NewTabSettingsPatch),
      "Invalid or reserved patch is rejected",
    );
  }
  const internal = mergeNewTabSettings(settings, {
    speedDial: { rootGuid: "rootFolder__", commonGuid: "commonGroup_" },
  }, true);
  assertEquals(
    internal.speedDial.rootGuid,
    "rootFolder__",
    "Internal migration can establish root GUIDs",
  );
}

async function testCompetingWriters(): Promise<void> {
  const fake = fakeStore();
  const first = fake.store.get();
  const saves = await Promise.allSettled([
    fake.store.patch(first.revision, { home: { showFeed: false } }),
    fake.store.patch(first.revision, { sponsors: { enabled: false } }),
  ]);
  assertEquals(saves[0].status, "fulfilled", "First queued update succeeds");
  assertEquals(
    saves[1].status,
    "rejected",
    "Stale competing update is rejected",
  );
  const current = fake.store.get();
  const retried = await fake.store.patch(current.revision, {
    sponsors: { enabled: false },
  });
  assertEquals(
    retried.home.showFeed,
    false,
    "Retry preserves the other writer's field",
  );
  assertEquals(retried.sponsors.enabled, false, "Retry changes its own field");
}

async function testLegacyWriterAndSingleNotification(): Promise<void> {
  const fake = fakeStore();
  const notifications: number[] = [];
  const unsub = fake.store.subscribe((settings) =>
    notifications.push(settings.revision)
  );
  const first = fake.store.get();
  await fake.store.patch(first.revision, { home: { showFeed: false } });
  assertEquals(
    notifications.length,
    1,
    "Own pref observer and writer publish once",
  );
  const external = JSON.parse(fake.raw()!) as { background: { type: string } };
  external.background.type = "none";
  // Legacy writes may preserve, but do not advance, the stored revision.
  fake.externalWrite(JSON.stringify(external));
  assertEquals(notifications.length, 2, "External writer is published");
  assert(
    notifications[1] > notifications[0],
    "Legacy write advances the effective revision",
  );
  let rejected = false;
  try {
    await fake.store.patch(notifications[0], { home: { showFeed: true } });
  } catch {
    rejected = true;
  }
  assert(rejected, "Patch based on the pre-legacy snapshot is rejected");
  assertEquals(
    fake.store.get().background.type,
    "none",
    "The external value is preserved",
  );
  unsub();
  fake.store.dispose();
}

async function testWriteFailureAndCloneIsolation(): Promise<void> {
  const fake = fakeStore();
  const initial = fake.store.get();
  initial.background.type = "custom";
  assertEquals(
    fake.store.get().background.type,
    "random",
    "Read snapshots cannot mutate the owner",
  );
  fake.failWrites();
  let rejected = false;
  try {
    await fake.store.patch(initial.revision, { home: { showFeed: false } });
  } catch {
    rejected = true;
  }
  assert(rejected, "Failed persistence is rejected");
  assertEquals(
    fake.raw(),
    null,
    "Failed save does not create partial settings",
  );
}

async function testNativeObserverOrdering(): Promise<void> {
  const pref = `floorp.test.newtab.settings.${Services.uuid.generateUUID()}`;
  const store = new NewTabSettingsStore({
    read: () =>
      Services.prefs.prefHasUserValue(pref)
        ? Services.prefs.getStringPref(pref)
        : null,
    write: (raw) => Services.prefs.setStringPref(pref, raw),
    observe(listener) {
      Services.prefs.addObserver(pref, listener);
      return () => Services.prefs.removeObserver(pref, listener);
    },
  });
  const directReads: number[] = [];
  const notifications: number[] = [];
  const errors: unknown[] = [];
  const consumer = () => {
    try {
      directReads.push(store.get().revision);
    } catch (error) {
      errors.push(error);
    }
  };
  const unsubscribe = store.subscribe((settings) =>
    notifications.push(settings.revision)
  );
  Services.prefs.addObserver(pref, consumer);
  try {
    const saved = await store.patch(store.get().revision, {
      background: { type: "none" },
    });
    assertEquals(errors.length, 0, "Native observers can read during a save");
    assertEquals(notifications.length, 1, "A native pref save publishes once");
    assertEquals(
      directReads.at(-1),
      saved.revision,
      "Native observers see the committed revision",
    );
    const external = JSON.parse(Services.prefs.getStringPref(pref)) as {
      components: { clock: boolean };
    };
    external.components.clock = false;
    Services.prefs.setStringPref(pref, JSON.stringify(external));
    assertEquals(notifications.length, 2, "Native legacy writes publish once");
    assertEquals(
      store.get().components.clock,
      false,
      "Native external changes are retained",
    );
    assert(
      store.get().revision > saved.revision,
      "Native legacy changes advance effective revision",
    );
  } finally {
    Services.prefs.removeObserver(pref, consumer);
    unsubscribe();
    store.dispose();
    Services.prefs.clearUserPref(pref);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "native observers see one coherent committed revision",
      fn: testNativeObserverOrdering,
    },
    {
      name: "legacy and unknown settings are retained",
      fn: testLegacyAndUnknownFields,
    },
    {
      name: "corrupt and future values remain untouched",
      fn: testInvalidDataIsNotReplaced,
    },
    {
      name: "page patches cannot redefine managed scope",
      fn: testPatchBoundary,
    },
    { name: "competing writers cannot lose fields", fn: testCompetingWriters },
    {
      name: "legacy writes advance revision and own writes notify once",
      fn: testLegacyWriterAndSingleNotification,
    },
    {
      name: "failed persistence is not reported as success",
      fn: testWriteFailureAndCloneIsolation,
    },
  ];
  await runTests("NewTabSettingsService.test.mts", tests);
}
