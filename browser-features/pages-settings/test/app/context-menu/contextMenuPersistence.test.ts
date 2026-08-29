// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { ContextMenuConfig } from "../../../../chrome/common/context-menu/types.ts";
import {
  parseContextMenuConfig,
  serializeContextMenuConfig,
} from "../../../../chrome/common/context-menu/config.ts";
import { createContextMenuPersistence } from "../../../src/app/context-menu/configPersistence.ts";
import {
  type ContextMenuLevelTarget,
  createDefaultContextMenuConfig,
  getContextMenuLevelOverride,
  getContextMenuMoveTargetKey,
  isContextMenuItemHideable,
  isContextMenuItemMovable,
  isContextMenuItemOrderAnchor,
  moveContextMenuItemBeforeKey,
  projectContextMenuItemKeysIntoNativeSlots,
  reorderContextMenuItemByKey,
  resetContextMenuProfile,
  setContextMenuItemHidden,
  setContextMenuProfileIndependent,
} from "../../../src/app/context-menu/operations.ts";

const TARGET: ContextMenuLevelTarget = {
  surfaceKey: "content",
  profileKey: "link",
  containerKey: "root",
};

const SEPARATOR_CATALOG_ITEMS = [
  {
    key: "item-a",
    customizable: true,
    movable: true,
    orderAnchor: true,
  },
  {
    key: "stable-separator",
    customizable: false,
    movable: true,
    orderAnchor: true,
  },
  {
    key: "item-b",
    customizable: true,
    movable: true,
    orderAnchor: true,
  },
] as const;

function createDeferred(): {
  promise: Promise<void>;
  resolve(): void;
} {
  let resolve!: () => void;
  const promise = new Promise<void>((resolvePromise) => {
    resolve = resolvePromise;
  });
  return { promise, resolve };
}

async function testProjectedStateAndSerialCommit(): Promise<void> {
  const firstWrite = createDeferred();
  const writes: string[] = [];
  const persistence = createContextMenuPersistence(
    { enabled: true, config: createDefaultContextMenuConfig() },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: async (_expected, serialized) => {
        writes.push(serialized);
        if (writes.length === 1) await firstWrite.promise;
        return { updated: true, currentValue: serialized };
      },
    },
  );

  const first = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "item-a", true)
  );
  const second = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "item-b", true)
  );

  const projectedHidden = getContextMenuLevelOverride(
    persistence.getSnapshot().projected.config,
    TARGET,
  ).hidden ?? [];
  const committedHidden = getContextMenuLevelOverride(
    persistence.getSnapshot().committed.config,
    TARGET,
  ).hidden ?? [];
  assertEquals(
    projectedHidden.join(","),
    "item-a,item-b",
    "projected state applies every queued key operation immediately",
  );
  assertEquals(
    committedHidden.length,
    0,
    "committed state does not advance before the writer succeeds",
  );
  assertEquals(writes.length, 1, "only the head operation writes initially");

  firstWrite.resolve();
  assertEquals(await first, true, "first queued update commits");
  assertEquals(await second, true, "second queued update commits");
  assertEquals(writes.length, 2, "queued writes run serially");

  const saved = parseContextMenuConfig(writes[1]);
  assertEquals(
    (getContextMenuLevelOverride(saved, TARGET).hidden ?? []).join(","),
    "item-a,item-b",
    "second write is based on the first committed result",
  );
}

async function testFailureRebasesLaterKeyOperation(): Promise<void> {
  const writes: string[] = [];
  const persistence = createContextMenuPersistence(
    { enabled: true, config: createDefaultContextMenuConfig() },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: (_expected, serialized) => {
        writes.push(serialized);
        return writes.length === 1
          ? Promise.reject(new Error("first write failed"))
          : Promise.resolve({ updated: true, currentValue: serialized });
      },
    },
  );

  const failed = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "failed-item", true)
  );
  const recovered = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "saved-item", true)
  );

  assertEquals(await failed, false, "failed write reports failure");
  assertEquals(await recovered, true, "the later operation still saves");
  const recoveredConfig = parseContextMenuConfig(writes[1]);
  const hidden = getContextMenuLevelOverride(
    recoveredConfig,
    TARGET,
  ).hidden ?? [];
  assertEquals(
    hidden.join(","),
    "saved-item",
    "later key operation rebases without the failed optimistic change",
  );
  assertEquals(
    persistence.getSnapshot().error,
    null,
    "a successful recovery clears the old writer error",
  );
}

async function testEnabledAndConfigUseOneQueue(): Promise<void> {
  const enabledWrite = createDeferred();
  const operations: string[] = [];
  const persistence = createContextMenuPersistence(
    { enabled: true, config: createDefaultContextMenuConfig() },
    {
      compareAndSetEnabled: async (_expected, enabled) => {
        operations.push("enabled");
        await enabledWrite.promise;
        return { updated: true, currentValue: enabled };
      },
      compareAndSetConfig: (_expected, serialized) => {
        operations.push("config");
        return Promise.resolve({ updated: true, currentValue: serialized });
      },
    },
  );

  const toggle = persistence.updateEnabled(false);
  const itemChange = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "item-a", true)
  );
  assertEquals(
    operations.join(","),
    "enabled",
    "config writer waits for the enabled writer",
  );
  assertEquals(
    persistence.getSnapshot().projected.enabled,
    false,
    "enabled projection updates while the bool write is pending",
  );
  assertEquals(
    getContextMenuLevelOverride(
      persistence.getSnapshot().projected.config,
      TARGET,
    ).hidden?.[0],
    "item-a",
    "config projection updates behind the bool write",
  );

  enabledWrite.resolve();
  assertEquals(await toggle, true, "enabled write commits");
  assertEquals(await itemChange, true, "config write commits afterwards");
  assertEquals(
    operations.join(","),
    "enabled,config",
    "bool and JSON preferences share one serial queue",
  );
}

async function testConcurrentClientsDetectAndRecoverFromConflict(): Promise<
  void
> {
  const initialConfig = createDefaultContextMenuConfig();
  let storedConfig = serializeContextMenuConfig(initialConfig);
  const createWriters = () => ({
    compareAndSetEnabled: (_expected: boolean | null, enabled: boolean) =>
      Promise.resolve({ updated: true, currentValue: enabled }),
    compareAndSetConfig: (expected: string | null, serialized: string) => {
      if (storedConfig !== expected) {
        return Promise.resolve({
          updated: false,
          currentValue: storedConfig,
        });
      }
      storedConfig = serialized;
      return Promise.resolve({ updated: true, currentValue: serialized });
    },
  });
  const versions = { enabled: true, config: storedConfig };
  const firstClient = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    createWriters(),
    versions,
  );
  const secondClient = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    createWriters(),
    versions,
  );

  assertEquals(
    await firstClient.updateConfig((config) =>
      setContextMenuItemHidden(config, TARGET, "item-a", true)
    ),
    true,
    "the first client commits against the shared baseline",
  );
  assertEquals(
    await secondClient.updateConfig((config) =>
      setContextMenuItemHidden(config, TARGET, "item-b", true)
    ),
    false,
    "a stale client reports a conflict instead of overwriting newer data",
  );
  assertEquals(
    secondClient.getSnapshot().error?.kind,
    "conflict",
    "the stale client exposes a distinct conflict state",
  );
  assertEquals(
    (getContextMenuLevelOverride(
      secondClient.getSnapshot().committed.config,
      TARGET,
    ).hidden ?? []).join(","),
    "item-a",
    "the stale client adopts the latest saved configuration",
  );

  assertEquals(
    await secondClient.updateConfig((config) =>
      setContextMenuItemHidden(config, TARGET, "item-b", true)
    ),
    true,
    "retrying the user operation succeeds against the refreshed baseline",
  );
  assertEquals(
    (getContextMenuLevelOverride(
      parseContextMenuConfig(storedConfig),
      TARGET,
    ).hidden ?? []).join(","),
    "item-a,item-b",
    "retrying preserves both clients' changes",
  );
  assertEquals(
    secondClient.getSnapshot().error,
    null,
    "a successful retry clears the conflict message",
  );
}

async function testConflictDropsQueuedStaleOperations(): Promise<void> {
  const initialConfig = createDefaultContextMenuConfig();
  const initialSerialized = serializeContextMenuConfig(initialConfig);
  const gate = createDeferred();
  let storedConfig = initialSerialized;
  let compareCalls = 0;
  const persistence = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: async (expected, serialized) => {
        compareCalls++;
        await gate.promise;
        if (storedConfig !== expected) {
          return { updated: false, currentValue: storedConfig };
        }
        storedConfig = serialized;
        return { updated: true, currentValue: serialized };
      },
    },
    { enabled: true, config: initialSerialized },
  );

  const first = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "stale-a", true)
  );
  const second = persistence.updateConfig((config) =>
    setContextMenuItemHidden(config, TARGET, "stale-b", true)
  );
  storedConfig = serializeContextMenuConfig(
    setContextMenuItemHidden(initialConfig, TARGET, "external", true),
  );
  gate.resolve();

  assertEquals(await first, false, "the in-flight stale operation conflicts");
  assertEquals(await second, false, "queued stale operations are abandoned");
  assertEquals(compareCalls, 1, "no stale queued write reaches the preference");
  assertEquals(
    (getContextMenuLevelOverride(
      parseContextMenuConfig(storedConfig),
      TARGET,
    ).hidden ?? []).join(","),
    "external",
    "an external reset or edit cannot be overwritten by an older queue",
  );
}

async function testUnsafeExternalConfigStopsAllRetries(): Promise<void> {
  const initialConfig = createDefaultContextMenuConfig();
  const initialSerialized = serializeContextMenuConfig(initialConfig);
  let storedConfig = "[]";
  let compareCalls = 0;
  const persistence = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: (expected, serialized) => {
        compareCalls++;
        if (storedConfig !== expected) {
          return Promise.resolve({
            updated: false,
            currentValue: storedConfig,
          });
        }
        storedConfig = serialized;
        return Promise.resolve({ updated: true, currentValue: serialized });
      },
    },
    { enabled: true, config: initialSerialized },
  );

  for (const itemKey of ["first-attempt", "second-attempt"]) {
    assertEquals(
      await persistence.updateConfig((config) =>
        setContextMenuItemHidden(config, TARGET, itemKey, true)
      ),
      false,
      "an unsafe external value rejects every ordinary editor write",
    );
    assertEquals(
      persistence.getSnapshot().error?.kind,
      "unsafe-config",
      "the editor receives a blocking unsafe-config state",
    );
  }
  assertEquals(compareCalls, 2, "both attempts compare without overwriting");
  assertEquals(
    await persistence.updateEnabled(false),
    true,
    "the independent enabled preference remains writable",
  );
  assertEquals(
    persistence.getSnapshot().blockingErrors.config?.kind,
    "unsafe-config",
    "an enabled write cannot clear the blocking configuration error",
  );
  assertEquals(
    storedConfig,
    "[]",
    "ordinary retries never replace an invalid external configuration",
  );
}

async function testWrongTypePreferenceCannotMatchAbsentVersion(): Promise<
  void
> {
  const initialConfig = createDefaultContextMenuConfig();
  const persistence = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: () =>
        Promise.resolve({
          updated: false,
          currentValue: null,
          typeMismatch: true,
        }),
    },
    { enabled: true, config: null },
  );

  assertEquals(
    await persistence.updateConfig((config) =>
      setContextMenuItemHidden(config, TARGET, "item-a", true)
    ),
    false,
    "a wrong-type preference rejects the attempted write",
  );
  assertEquals(
    persistence.getSnapshot().error?.kind,
    "preference-type-mismatch",
    "the UI receives a blocking preference-type error",
  );
  assertEquals(
    await persistence.updateEnabled(false),
    true,
    "a config type mismatch does not disable the independent enabled preference",
  );
  assertEquals(
    persistence.getSnapshot().blockingErrors.config?.kind,
    "preference-type-mismatch",
    "an enabled write cannot clear the config type mismatch",
  );
  assertEquals(
    getContextMenuLevelOverride(
      persistence.getSnapshot().committed.config,
      TARGET,
    ).hidden?.length ?? 0,
    0,
    "the rejected optimistic value is not committed",
  );
}

async function testEnabledTypeMismatchSurvivesConfigWrite(): Promise<void> {
  const initialConfig = createDefaultContextMenuConfig();
  const persistence = createContextMenuPersistence(
    { enabled: true, config: initialConfig },
    {
      compareAndSetEnabled: () =>
        Promise.resolve({
          updated: false,
          currentValue: null,
          typeMismatch: true,
        }),
      compareAndSetConfig: (_expected, serialized) =>
        Promise.resolve({ updated: true, currentValue: serialized }),
    },
  );

  assertEquals(
    await persistence.updateEnabled(false),
    false,
    "a wrong-type enabled preference rejects the toggle",
  );
  assertEquals(
    await persistence.updateConfig((config) =>
      setContextMenuItemHidden(config, TARGET, "item-a", true)
    ),
    true,
    "the independent config preference can still commit",
  );
  assertEquals(
    persistence.getSnapshot().blockingErrors.enabled?.kind,
    "preference-type-mismatch",
    "a config write cannot clear the blocking enabled preference error",
  );
}

function testReorderPreservesUnknownKeys(): void {
  const initial = setContextMenuItemHidden(
    createDefaultContextMenuConfig(),
    TARGET,
    "hidden-from-old-version",
    true,
  );
  initial.surfaces.content.base.root.order = [
    "item-a",
    "removed-by-firefox",
    "item-b",
  ];

  const reordered = reorderContextMenuItemByKey(
    initial,
    TARGET,
    [
      { key: "item-a", customizable: true },
      { key: "item-b", customizable: true },
      { key: "new-firefox-item", customizable: true },
    ],
    "new-firefox-item",
    "item-a",
  );
  const level = reordered.surfaces.content.base.root;
  assertEquals(
    level.order?.join(","),
    "new-firefox-item,item-a,removed-by-firefox,item-b",
    "key reorder retains temporarily unknown Firefox item keys",
  );
  assertEquals(
    level.hidden?.join(","),
    "hidden-from-old-version",
    "reordering does not overwrite the visibility overlay",
  );
}

function testNativeSlotProjectionKeepsFixedItemsInPlace(): void {
  const catalogItems = [
    { key: "item-a", customizable: true },
    { key: "fixed-separator", customizable: false },
    { key: "item-b", customizable: true },
  ];
  const config = createDefaultContextMenuConfig();
  config.surfaces.content = {
    base: {
      root: {
        // Include a fixed key to cover cleanup of configuration written by an
        // older Hub implementation.
        order: ["item-a", "fixed-separator", "item-b"],
      },
    },
    profiles: {},
  };

  const reordered = reorderContextMenuItemByKey(
    config,
    TARGET,
    catalogItems,
    "item-a",
    "item-b",
  );
  const level = reordered.surfaces.content.base.root;
  assertEquals(
    level.order?.join(","),
    "item-b,item-a",
    "saved order contains only catalog items marked customizable",
  );
  assertEquals(
    projectContextMenuItemKeysIntoNativeSlots(catalogItems, level).join(","),
    "item-b,fixed-separator,item-a",
    "Hub projection substitutes movable keys into their native slots",
  );
}

function testSeparatorParticipatesInSavedOrder(): void {
  const reordered = reorderContextMenuItemByKey(
    createDefaultContextMenuConfig(),
    TARGET,
    SEPARATOR_CATALOG_ITEMS,
    "item-a",
    "stable-separator",
  );
  const level = reordered.surfaces.content.base.root;
  assertEquals(
    level.order?.join(","),
    "stable-separator,item-a,item-b",
    "a stable separator can be dragged across a command and is persisted",
  );
  assertEquals(
    projectContextMenuItemKeysIntoNativeSlots(
      SEPARATOR_CATALOG_ITEMS,
      level,
    ).join(","),
    "stable-separator,item-a,item-b",
    "the Hub projects the persisted separator position",
  );
}

async function testSeparatorOrderIsPersisted(): Promise<void> {
  const writes: string[] = [];
  const persistence = createContextMenuPersistence(
    { enabled: true, config: createDefaultContextMenuConfig() },
    {
      compareAndSetEnabled: (_expected, enabled) =>
        Promise.resolve({ updated: true, currentValue: enabled }),
      compareAndSetConfig: (_expected, serialized) => {
        writes.push(serialized);
        return Promise.resolve({ updated: true, currentValue: serialized });
      },
    },
  );

  const saved = await persistence.updateConfig((config) =>
    reorderContextMenuItemByKey(
      config,
      TARGET,
      SEPARATOR_CATALOG_ITEMS,
      "item-a",
      "stable-separator",
    )
  );
  assertEquals(saved, true, "the separator reorder write succeeds");
  const serializedLevel = getContextMenuLevelOverride(
    parseContextMenuConfig(writes[0]),
    TARGET,
  );
  assertEquals(
    serializedLevel.order?.join(","),
    "stable-separator,item-a,item-b",
    "the serialized preference retains the separator position",
  );
}

function testSplitCapabilitiesAndLegacyFallback(): void {
  const separator = {
    customizable: false,
    movable: true,
    hideable: false,
    orderAnchor: true,
  };
  assert(
    isContextMenuItemMovable(separator),
    "a stable separator exposes an enabled drag handle",
  );
  assert(
    !isContextMenuItemHideable(separator),
    "a separator does not expose a visibility switch",
  );
  assert(
    isContextMenuItemOrderAnchor(separator),
    "a stable separator can receive a drop",
  );

  const legacyItem = { customizable: true };
  assert(
    isContextMenuItemMovable(legacyItem) &&
      isContextMenuItemHideable(legacyItem) &&
      isContextMenuItemOrderAnchor(legacyItem),
    "legacy catalogs retain their combined customization behavior",
  );
}

function testProtectedItemKeepsNativeSlot(): void {
  const catalogItems = [
    {
      key: "item-a",
      customizable: true,
      movable: true,
      orderAnchor: true,
    },
    {
      key: "protected-anchor",
      customizable: false,
      movable: false,
      orderAnchor: false,
    },
    {
      key: "item-b",
      customizable: true,
      movable: true,
      orderAnchor: true,
    },
  ];

  const rejectedTarget = reorderContextMenuItemByKey(
    createDefaultContextMenuConfig(),
    TARGET,
    catalogItems,
    "item-a",
    "protected-anchor",
  );
  assertEquals(
    rejectedTarget.surfaces.content?.base.root?.order,
    undefined,
    "a protected native node cannot be used as a drop target",
  );

  const rejected = reorderContextMenuItemByKey(
    createDefaultContextMenuConfig(),
    TARGET,
    catalogItems,
    "protected-anchor",
    "item-a",
  );
  assertEquals(
    rejected.surfaces.content?.base.root?.order,
    undefined,
    "a protected native node cannot itself be dragged",
  );
}

function testButtonMoveSkipsProtectedItemAndMovesSeparator(): void {
  const catalogItems = [
    {
      key: "item-a",
      customizable: true,
      movable: true,
      orderAnchor: true,
    },
    {
      key: "protected-item",
      customizable: false,
      movable: false,
      orderAnchor: false,
    },
    {
      key: "stable-separator",
      customizable: false,
      movable: true,
      orderAnchor: true,
    },
    {
      key: "item-b",
      customizable: true,
      movable: true,
      orderAnchor: true,
    },
  ] as const;

  const downTarget = getContextMenuMoveTargetKey(
    catalogItems,
    "item-a",
    "down",
  );
  assertEquals(
    downTarget,
    "stable-separator",
    "the down button skips a protected item and targets the separator",
  );
  assert(downTarget !== undefined, "the button move has a valid target");
  assertEquals(
    getContextMenuMoveTargetKey(catalogItems, "stable-separator", "up"),
    "item-a",
    "a stable separator receives an enabled up-button target",
  );
  assertEquals(
    getContextMenuMoveTargetKey(catalogItems, "protected-item", "down"),
    undefined,
    "a protected item never receives a button-move target",
  );

  const reordered = reorderContextMenuItemByKey(
    createDefaultContextMenuConfig(),
    TARGET,
    catalogItems,
    "item-a",
    downTarget,
  );
  assertEquals(
    projectContextMenuItemKeysIntoNativeSlots(
      catalogItems,
      getContextMenuLevelOverride(reordered, TARGET),
    ).join(","),
    "stable-separator,protected-item,item-a,item-b",
    "button reorder moves stable anchors while preserving the protected native slot",
  );
}

function testExplicitGapMoveBetweenNativeHiddenItems(): void {
  const catalogItems = [
    {
      key: "moved-item",
      customizable: true,
      movable: true,
      orderAnchor: true,
      nativeHidden: false,
    },
    {
      key: "conditional-a",
      customizable: true,
      movable: true,
      orderAnchor: true,
      nativeHidden: true,
    },
    {
      key: "conditional-b",
      customizable: true,
      movable: true,
      orderAnchor: true,
      nativeHidden: true,
    },
  ] as const;

  const moved = moveContextMenuItemBeforeKey(
    createDefaultContextMenuConfig(),
    TARGET,
    catalogItems,
    "moved-item",
    "conditional-b",
  );
  assertEquals(
    getContextMenuLevelOverride(moved, TARGET).order?.join(","),
    "conditional-a,moved-item,conditional-b",
    "an explicit gap can place an item between two Firefox-conditional items",
  );
}

function testExplicitGapMoveToBeginningAndEnd(): void {
  const catalogItems = [
    { key: "item-a", customizable: true, orderAnchor: true },
    { key: "item-b", customizable: true, orderAnchor: true },
    { key: "item-c", customizable: true, orderAnchor: true },
  ];
  const movedToBeginning = moveContextMenuItemBeforeKey(
    createDefaultContextMenuConfig(),
    TARGET,
    catalogItems,
    "item-c",
    "item-a",
  );
  assertEquals(
    getContextMenuLevelOverride(movedToBeginning, TARGET).order?.join(","),
    "item-c,item-a,item-b",
    "the first gap places an item at the beginning",
  );

  const movedToEnd = moveContextMenuItemBeforeKey(
    movedToBeginning,
    TARGET,
    catalogItems,
    "item-c",
    null,
  );
  assertEquals(
    getContextMenuLevelOverride(movedToEnd, TARGET).order?.join(","),
    "item-a,item-b,item-c",
    "a null anchor places an item after the final anchor",
  );
}

function testExplicitGapMoveKeepsProtectedNativeSlot(): void {
  const catalogItems = [
    { key: "item-a", customizable: true, orderAnchor: true },
    {
      key: "protected-item",
      customizable: false,
      movable: false,
      orderAnchor: false,
    },
    { key: "item-b", customizable: true, orderAnchor: true },
    { key: "item-c", customizable: true, orderAnchor: true },
  ];
  const config = createDefaultContextMenuConfig();
  config.surfaces.content = {
    base: {
      root: {
        order: [
          "item-a",
          "removed-by-firefox",
          "protected-item",
          "item-b",
          "item-c",
        ],
      },
    },
    profiles: {},
  };

  const moved = moveContextMenuItemBeforeKey(
    config,
    TARGET,
    catalogItems,
    "item-c",
    "item-a",
  );
  const level = getContextMenuLevelOverride(moved, TARGET);
  assertEquals(
    level.order?.join(","),
    "item-c,item-a,removed-by-firefox,item-b",
    "the move retains unknown saved keys and removes a known protected key",
  );
  assertEquals(
    projectContextMenuItemKeysIntoNativeSlots(catalogItems, level).join(","),
    "item-c,protected-item,item-a,item-b",
    "the protected item keeps its exact native slot",
  );
}

function testExplicitGapMoveRejectsInvalidTarget(): void {
  const catalogItems = [
    { key: "item-a", customizable: true, orderAnchor: true },
    {
      key: "protected-item",
      customizable: false,
      movable: false,
      orderAnchor: false,
    },
    { key: "item-b", customizable: true, orderAnchor: true },
  ];
  const config = createDefaultContextMenuConfig();

  assert(
    moveContextMenuItemBeforeKey(
      config,
      TARGET,
      catalogItems,
      "item-a",
      "missing-item",
    ) === config,
    "an unknown target is an identity no-op",
  );
  assert(
    moveContextMenuItemBeforeKey(
      config,
      TARGET,
      catalogItems,
      "item-a",
      "protected-item",
    ) === config,
    "a protected item cannot be used as the gap anchor",
  );
}

function testNewFirefoxItemKeepsNativeSlot(): void {
  const catalogItems = [
    { key: "item-a", customizable: true, orderAnchor: true },
    { key: "new-firefox-item", customizable: true, orderAnchor: true },
    { key: "item-b", customizable: true, orderAnchor: true },
  ];
  const config = createDefaultContextMenuConfig();
  config.surfaces.content = {
    base: { root: { order: ["item-b", "item-a"] } },
    profiles: {},
  };

  assertEquals(
    projectContextMenuItemKeysIntoNativeSlots(
      catalogItems,
      config.surfaces.content.base.root,
    ).join(","),
    "item-b,new-firefox-item,item-a",
    "a Firefox item absent from the saved order keeps its native slot",
  );
}

function testProfileSwitchRetainsDormantContainers(): void {
  const config: ContextMenuConfig = {
    schemaVersion: 1,
    surfaces: {
      content: {
        base: {},
        profiles: {
          link: {
            independent: true,
            containers: { root: { hidden: ["item-a"] } },
          },
        },
      },
    },
  };

  const shared = setContextMenuProfileIndependent(
    config,
    "content",
    "link",
    false,
  );
  assertEquals(
    shared.surfaces.content.profiles.link.containers.root.hidden?.[0],
    "item-a",
    "turning independence off retains the dormant profile layout",
  );
  const independentAgain = setContextMenuProfileIndependent(
    shared,
    "content",
    "link",
    true,
  );
  assertEquals(
    independentAgain.surfaces.content.profiles.link.containers.root.hidden?.[0],
    "item-a",
    "turning independence back on restores the retained layout",
  );
}

function testProfileResetPreservesUnrelatedChanges(): void {
  const config: ContextMenuConfig = {
    schemaVersion: 1,
    surfaces: {
      content: {
        base: { root: { hidden: ["shared-item"] } },
        profiles: {
          link: { independent: true, containers: {} },
          image: { independent: true, containers: {} },
        },
      },
      tab: {
        base: { root: { order: ["tab-item"] } },
        profiles: {},
      },
    },
  };

  const reset = resetContextMenuProfile(config, "content", "link");
  assert(
    reset.surfaces.content.profiles.link === undefined,
    "selected profile is removed",
  );
  assert(
    reset.surfaces.content.profiles.image !== undefined,
    "another profile is retained",
  );
  assertEquals(
    reset.surfaces.content.base.root.hidden?.[0],
    "shared-item",
    "shared layout is retained",
  );
  assertEquals(
    reset.surfaces.tab.base.root.order?.[0],
    "tab-item",
    "another surface is retained",
  );
}

const tests: TestCase[] = [
  {
    name: "projected state and serial commit",
    fn: testProjectedStateAndSerialCommit,
  },
  {
    name: "failure rebases later key operation",
    fn: testFailureRebasesLaterKeyOperation,
  },
  {
    name: "enabled and config updates use one queue",
    fn: testEnabledAndConfigUseOneQueue,
  },
  {
    name: "concurrent clients detect and recover from conflicts",
    fn: testConcurrentClientsDetectAndRecoverFromConflict,
  },
  {
    name: "conflicts drop queued stale operations",
    fn: testConflictDropsQueuedStaleOperations,
  },
  {
    name: "unsafe external configs stop ordinary retries",
    fn: testUnsafeExternalConfigStopsAllRetries,
  },
  {
    name: "wrong-type preferences cannot match absent versions",
    fn: testWrongTypePreferenceCannotMatchAbsentVersion,
  },
  {
    name: "enabled type mismatch survives config writes",
    fn: testEnabledTypeMismatchSurvivesConfigWrite,
  },
  {
    name: "key reorder preserves unknown catalog entries",
    fn: testReorderPreservesUnknownKeys,
  },
  {
    name: "native-slot projection keeps fixed items in place",
    fn: testNativeSlotProjectionKeepsFixedItemsInPlace,
  },
  {
    name: "stable separator participates in saved order",
    fn: testSeparatorParticipatesInSavedOrder,
  },
  {
    name: "stable separator order is persisted",
    fn: testSeparatorOrderIsPersisted,
  },
  {
    name: "split capabilities and legacy fallback",
    fn: testSplitCapabilitiesAndLegacyFallback,
  },
  {
    name: "protected item keeps its native slot",
    fn: testProtectedItemKeepsNativeSlot,
  },
  {
    name: "button move skips protected items and moves separators",
    fn: testButtonMoveSkipsProtectedItemAndMovesSeparator,
  },
  {
    name: "explicit gap move supports Firefox-conditional items",
    fn: testExplicitGapMoveBetweenNativeHiddenItems,
  },
  {
    name: "explicit gap move supports beginning and end",
    fn: testExplicitGapMoveToBeginningAndEnd,
  },
  {
    name: "explicit gap move keeps protected native slots",
    fn: testExplicitGapMoveKeepsProtectedNativeSlot,
  },
  {
    name: "explicit gap move rejects invalid targets",
    fn: testExplicitGapMoveRejectsInvalidTarget,
  },
  {
    name: "new Firefox item keeps its native slot",
    fn: testNewFirefoxItemKeepsNativeSlot,
  },
  {
    name: "profile switch retains dormant containers",
    fn: testProfileSwitchRetainsDormantContainers,
  },
  {
    name: "profile reset preserves unrelated settings",
    fn: testProfileResetPreservesUnrelatedChanges,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("contextMenuPersistence.test.ts", tests);
}
