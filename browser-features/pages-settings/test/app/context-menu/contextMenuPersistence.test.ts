// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { ContextMenuConfig } from "../../../../chrome/common/context-menu/types.ts";
import { parseContextMenuConfig } from "../../../../chrome/common/context-menu/config.ts";
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
      writeEnabled: () => Promise.resolve(),
      writeConfig: (serialized) => {
        writes.push(serialized);
        return writes.length === 1 ? firstWrite.promise : Promise.resolve();
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
      writeEnabled: () => Promise.resolve(),
      writeConfig: (serialized) => {
        writes.push(serialized);
        return writes.length === 1
          ? Promise.reject(new Error("first write failed"))
          : Promise.resolve();
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
      writeEnabled: () => {
        operations.push("enabled");
        return enabledWrite.promise;
      },
      writeConfig: () => {
        operations.push("config");
        return Promise.resolve();
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
      writeEnabled: () => Promise.resolve(),
      writeConfig: (serialized) => {
        writes.push(serialized);
        return Promise.resolve();
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
