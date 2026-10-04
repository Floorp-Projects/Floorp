// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  createDefaultMouseGestureConfig,
  createGestureConfigPersistence,
  normalizeMouseGestureConfig,
} from "../../../src/app/gesture/configPersistence.ts";

function createDeferred(): {
  promise: Promise<void>;
  resolve: () => void;
} {
  let resolve!: () => void;
  const promise = new Promise<void>((resolvePromise) => {
    resolve = resolvePromise;
  });
  return { promise, resolve };
}

function testEnabledUpdateWritesOnlyBool(): Promise<void> {
  const boolWrites: boolean[] = [];
  const configWrites: string[] = [];
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(false),
    {
      writeEnabled: (enabled) => {
        boolWrites.push(enabled);
        return Promise.resolve();
      },
      writeConfig: (serialized) => {
        configWrites.push(serialized);
        return Promise.resolve();
      },
    },
  );

  return persistence.updateEnabled(true).then((saved) => {
    assertEquals(saved, true, "enabled update succeeds");
    assertEquals(boolWrites.length, 1, "enabled update writes one bool pref");
    assertEquals(boolWrites[0], true, "enabled update writes requested value");
    assertEquals(configWrites.length, 0, "enabled update does not write JSON");
    assertEquals(
      persistence.getSnapshot().config.enabled,
      true,
      "enabled state commits after the bool write",
    );
  });
}

function testConfigUpdateWritesOnlyJsonWithoutEnabled(): Promise<void> {
  const boolWrites: boolean[] = [];
  const configWrites: string[] = [];
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(true),
    {
      writeEnabled: (enabled) => {
        boolWrites.push(enabled);
        return Promise.resolve();
      },
      writeConfig: (serialized) => {
        configWrites.push(serialized);
        return Promise.resolve();
      },
    },
  );

  return persistence.updateConfig({ showTrail: false }).then((saved) => {
    assertEquals(saved, true, "config update succeeds");
    assertEquals(
      boolWrites.length,
      0,
      "config update does not write bool pref",
    );
    assertEquals(configWrites.length, 1, "config update writes one JSON pref");
    const persisted = JSON.parse(configWrites[0]);
    assertEquals(
      Object.hasOwn(persisted, "enabled"),
      false,
      "JSON pref omits enabled",
    );
    assertEquals(persisted.showTrail, false, "JSON contains the config update");
  });
}

async function testRapidNestedUpdatesUseLastCommittedConfig(): Promise<void> {
  const persisted: Array<Record<string, unknown>> = [];
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(true),
    {
      writeEnabled: () => Promise.resolve(),
      writeConfig: async (serialized) => {
        await Promise.resolve();
        persisted.push(JSON.parse(serialized));
      },
    },
  );

  const minDistanceUpdate = persistence.updateConfig((current) => ({
    contextMenu: { ...current.contextMenu, minDistance: 24 },
  }));
  const timeoutUpdate = persistence.updateConfig((current) => ({
    contextMenu: { ...current.contextMenu, preventionTimeout: 850 },
  }));

  const results = await Promise.all([minDistanceUpdate, timeoutUpdate]);
  assert(
    results.every((result) => result),
    "both queued nested updates should succeed",
  );
  assertEquals(persisted.length, 2, "queued updates write in order");
  assertEquals(
    persistence.getSnapshot().config.contextMenu.minDistance,
    24,
    "second update retains first nested value",
  );
  assertEquals(
    persistence.getSnapshot().config.contextMenu.preventionTimeout,
    850,
    "second nested value commits",
  );
}

async function testRapidWheelUpdatesRetainBothActions(): Promise<void> {
  const persisted: Array<Record<string, unknown>> = [];
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(true),
    {
      writeEnabled: () => Promise.resolve(),
      writeConfig: (serialized) => {
        persisted.push(JSON.parse(serialized));
        return Promise.resolve();
      },
    },
  );

  const scrollUpUpdate = persistence.updateConfig((current) => ({
    wheelActions: {
      ...current.wheelActions,
      scrollUp: "gecko-zoom-in",
    },
  }));
  const scrollDownUpdate = persistence.updateConfig((current) => ({
    wheelActions: {
      ...current.wheelActions,
      scrollDown: "gecko-zoom-out",
    },
  }));

  const results = await Promise.all([scrollUpUpdate, scrollDownUpdate]);
  assert(
    results.every((result) => result),
    "both rapid wheel updates should succeed",
  );
  assertEquals(persisted.length, 2, "both wheel updates persist");
  const secondWheelActions = persisted[1].wheelActions as Record<
    string,
    unknown
  >;
  assertEquals(
    secondWheelActions.scrollUp,
    "gecko-zoom-in",
    "second persisted config retains scrollUp update",
  );
  assertEquals(
    secondWheelActions.scrollDown,
    "gecko-zoom-out",
    "second persisted config contains scrollDown update",
  );
  assertEquals(
    persistence.getSnapshot().config.wheelActions.scrollUp,
    "gecko-zoom-in",
    "committed config retains scrollUp update",
  );
  assertEquals(
    persistence.getSnapshot().config.wheelActions.scrollDown,
    "gecko-zoom-out",
    "committed config contains scrollDown update",
  );
}

async function testPendingAndCommitAfterSuccess(): Promise<void> {
  const firstDeferred = createDeferred();
  const secondDeferred = createDeferred();
  let writeIndex = 0;
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(true),
    {
      writeEnabled: () => Promise.resolve(),
      writeConfig: () =>
        writeIndex++ === 0 ? firstDeferred.promise : secondDeferred.promise,
    },
  );

  const firstSave = persistence.updateConfig({ trailColor: "#123456" });
  const secondSave = persistence.updateConfig({ showTrail: false });
  assertEquals(
    persistence.getSnapshot().pending,
    true,
    "two queued writes are pending",
  );
  assertEquals(
    persistence.getSnapshot().config.trailColor,
    "#37ff00",
    "UI config does not update before persistence succeeds",
  );

  firstDeferred.resolve();
  assertEquals(await firstSave, true, "first deferred write succeeds");
  await Promise.resolve();
  assertEquals(
    persistence.getSnapshot().pending,
    true,
    "pending remains true while the second write is queued",
  );
  assertEquals(
    persistence.getSnapshot().config.trailColor,
    "#123456",
    "first successful write commits while the second remains pending",
  );

  secondDeferred.resolve();
  assertEquals(await secondSave, true, "second deferred write succeeds");
  assertEquals(persistence.getSnapshot().pending, false, "pending clears");
  assertEquals(
    persistence.getSnapshot().config.trailColor,
    "#123456",
    "UI config commits after persistence succeeds",
  );
  assertEquals(
    persistence.getSnapshot().config.showTrail,
    false,
    "second queued write commits after it succeeds",
  );
}

async function testFailurePreservesCommittedConfig(): Promise<void> {
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(true),
    {
      writeEnabled: () => Promise.resolve(),
      writeConfig: () => Promise.reject(new Error("write failed")),
    },
  );

  const saved = await persistence.updateConfig({ showTrail: false });
  assertEquals(saved, false, "failed write reports false");
  assertEquals(
    persistence.getSnapshot().config.showTrail,
    true,
    "failed write does not commit UI config",
  );
  assertEquals(
    persistence.getSnapshot().pending,
    false,
    "failed write settles",
  );
  assert(
    persistence.getSnapshot().error?.includes("write failed"),
    "failed write exposes an error",
  );
}

async function testQueueRecoversAfterFailure(): Promise<void> {
  const operations: string[] = [];
  let configAttempts = 0;
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(false),
    {
      writeEnabled: () => {
        operations.push("enabled");
        return Promise.reject(new Error("bool failed"));
      },
      writeConfig: (serialized) => {
        operations.push("config");
        configAttempts++;
        JSON.parse(serialized);
        return Promise.resolve();
      },
    },
  );

  const failedToggle = persistence.updateEnabled(true);
  const recoveredConfig = persistence.updateConfig({ showLabel: false });
  assertEquals(await failedToggle, false, "first queued write fails");
  assertEquals(await recoveredConfig, true, "later queued write still runs");
  assertEquals(
    operations.join(","),
    "enabled,config",
    "one queue preserves order",
  );
  assertEquals(configAttempts, 1, "post-failure config writer runs once");
  assertEquals(
    persistence.getSnapshot().config.enabled,
    false,
    "recovery builds from last successful enabled state",
  );
  assertEquals(
    persistence.getSnapshot().config.showLabel,
    false,
    "post-failure update commits",
  );
  assertEquals(
    persistence.getSnapshot().error,
    null,
    "success clears old error",
  );
}

async function testBoolUpdateRecoversAfterConfigFailure(): Promise<void> {
  const operations: string[] = [];
  const boolWrites: boolean[] = [];
  const persistence = createGestureConfigPersistence(
    createDefaultMouseGestureConfig(false),
    {
      writeEnabled: (enabled) => {
        operations.push("enabled");
        boolWrites.push(enabled);
        return Promise.resolve();
      },
      writeConfig: () => {
        operations.push("config");
        return Promise.reject(new Error("json failed"));
      },
    },
  );

  const failedConfig = persistence.updateConfig({ showTrail: false });
  const recoveredToggle = persistence.updateEnabled(true);
  assertEquals(await failedConfig, false, "JSON write fails");
  assertEquals(await recoveredToggle, true, "later bool write still succeeds");
  assertEquals(
    operations.join(","),
    "config,enabled",
    "shared queue preserves JSON then bool order",
  );
  assertEquals(boolWrites.length, 1, "recovery writes only one bool pref");
  assertEquals(boolWrites[0], true, "recovery writes requested bool value");
  assertEquals(
    persistence.getSnapshot().config.showTrail,
    true,
    "failed JSON update remains uncommitted",
  );
  assertEquals(
    persistence.getSnapshot().config.enabled,
    true,
    "bool recovery commits from last successful config",
  );
  assertEquals(
    persistence.getSnapshot().error,
    null,
    "bool success clears error",
  );
}

function testDeepNormalization(): void {
  const normalized = normalizeMouseGestureConfig(
    {
      contextMenu: { minDistance: 21 },
      rockerActions: { leftRight: "gecko-reload" },
      wheelActions: {
        scrollUp: "gecko-close-tab",
        scrollDown: "gecko-zoom-in",
      },
    },
    true,
  );

  assertEquals(normalized.enabled, true, "separate enabled pref wins");
  assertEquals(normalized.contextMenu.minDistance, 21, "nested value survives");
  assertEquals(
    normalized.contextMenu.preventionTimeout,
    200,
    "missing nested context value receives default",
  );
  assertEquals(
    normalized.rockerActions.rightLeft,
    "gecko-back",
    "missing rocker action receives default",
  );
  assertEquals(
    normalized.wheelActions.scrollUp,
    "gecko-show-previous-tab",
    "unsafe wheel action is replaced with safe default",
  );
  assertEquals(
    normalized.wheelActions.scrollDown,
    "gecko-zoom-in",
    "repeat-safe wheel action is preserved",
  );
}

const tests: TestCase[] = [
  {
    name: "enabled update writes bool only",
    fn: testEnabledUpdateWritesOnlyBool,
  },
  {
    name: "config update writes JSON only without enabled",
    fn: testConfigUpdateWritesOnlyJsonWithoutEnabled,
  },
  {
    name: "rapid nested updates use last committed config",
    fn: testRapidNestedUpdatesUseLastCommittedConfig,
  },
  {
    name: "rapid wheel updates retain both actions",
    fn: testRapidWheelUpdatesRetainBothActions,
  },
  {
    name: "pending state commits only after success",
    fn: testPendingAndCommitAfterSuccess,
  },
  {
    name: "failure preserves committed config",
    fn: testFailurePreservesCommittedConfig,
  },
  {
    name: "queue recovers after failure",
    fn: testQueueRecoversAfterFailure,
  },
  {
    name: "bool update recovers after JSON failure",
    fn: testBoolUpdateRecoversAfterConfigFailure,
  },
  {
    name: "load normalization is deep and wheel-safe",
    fn: testDeepNormalization,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("configPersistence.test.ts", tests);
}
