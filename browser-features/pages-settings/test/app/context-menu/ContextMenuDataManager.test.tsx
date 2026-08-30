// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act } from "react";
import { createRoot, type Root } from "react-dom/client";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuCatalogSnapshot,
} from "../../../../chrome/common/context-menu/types.ts";
import { serializeContextMenuConfig } from "../../../../chrome/common/context-menu/config.ts";
import { rpc } from "../../../src/lib/rpc/rpc.ts";
import {
  type ContextMenuSettingsModel,
  useContextMenuSettings,
} from "../../../src/app/context-menu/dataManager.ts";
import {
  createDefaultContextMenuConfig,
  getContextMenuLevelOverride,
  setContextMenuItemHidden,
} from "../../../src/app/context-menu/operations.ts";

const EMPTY_CATALOG: ContextMenuCatalogSnapshot = {
  schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
  revision: 0,
  locale: "en-US",
  surfaces: [],
};

function createCatalog(revision: number, itemKey: string) {
  return {
    schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
    revision,
    locale: "en-US",
    surfaces: [{
      key: "browser.content",
      label: "Web page",
      profiles: [{
        key: "page",
        label: "Page",
        containers: [{
          key: "root",
          label: "Web page",
          complete: true,
          items: [{
            key: itemKey,
            label: itemKey,
            kind: "command" as const,
            source: "firefox" as const,
            customizable: true,
            movable: true,
            hideable: true,
            orderAnchor: true,
            nativeHidden: false,
          }],
        }],
      }],
    }],
  } satisfies ContextMenuCatalogSnapshot;
}

type RpcMethodName =
  | "getBoolPrefState"
  | "getStringPrefState"
  | "compareAndSetBoolPref"
  | "compareAndSetStringPref"
  | "getContextMenuCatalog"
  | "getContextMenuCatalogRevision";

type RpcOverrides = Pick<typeof rpc, RpcMethodName>;

interface RenderedModel {
  current(): ContextMenuSettingsModel;
  flush(): Promise<void>;
  cleanup(): void;
}

interface CapturedTimer {
  pending(): boolean;
  run(): void;
  restore(): void;
}

function captureTimeout(delay: number): CapturedTimer {
  const originalSetTimeout = globalThis.setTimeout;
  const originalClearTimeout = globalThis.clearTimeout;
  const capturedId = 2_147_000_001;
  let callback: (() => void) | null = null;
  globalThis.setTimeout = ((
    handler: unknown,
    timeout?: number,
    ...args: unknown[]
  ) => {
    if (timeout === delay && typeof handler === "function") {
      callback = () => Reflect.apply(handler, globalThis, args);
      return capturedId;
    }
    return Reflect.apply(originalSetTimeout, globalThis, [
      handler,
      timeout,
      ...args,
    ]) as number;
  }) as typeof globalThis.setTimeout;
  globalThis.clearTimeout = ((id?: number) => {
    if (id === capturedId) {
      callback = null;
      return;
    }
    Reflect.apply(originalClearTimeout, globalThis, [id]);
  }) as typeof globalThis.clearTimeout;
  return {
    pending: () => callback !== null,
    run: () => {
      const current = callback;
      callback = null;
      assert(current !== null, `a ${delay}ms timer should be pending`);
      current();
    },
    restore: () => {
      globalThis.setTimeout = originalSetTimeout;
      globalThis.clearTimeout = originalClearTimeout;
    },
  };
}

function captureInterval(delay: number): CapturedTimer {
  const originalSetInterval = globalThis.setInterval;
  const originalClearInterval = globalThis.clearInterval;
  const capturedId = 2_147_000_002;
  let callback: (() => void) | null = null;
  globalThis.setInterval = ((
    handler: unknown,
    timeout?: number,
    ...args: unknown[]
  ) => {
    if (timeout === delay && typeof handler === "function") {
      callback = () => Reflect.apply(handler, globalThis, args);
      return capturedId;
    }
    return Reflect.apply(originalSetInterval, globalThis, [
      handler,
      timeout,
      ...args,
    ]) as number;
  }) as typeof globalThis.setInterval;
  globalThis.clearInterval = ((id?: number) => {
    if (id === capturedId) {
      callback = null;
      return;
    }
    Reflect.apply(originalClearInterval, globalThis, [id]);
  }) as typeof globalThis.clearInterval;
  return {
    pending: () => callback !== null,
    run: () => {
      assert(callback !== null, `a ${delay}ms interval should be active`);
      callback();
    },
    restore: () => {
      globalThis.setInterval = originalSetInterval;
      globalThis.clearInterval = originalClearInterval;
    },
  };
}

function installRpcOverrides(overrides: RpcOverrides): () => void {
  const originals = Object.fromEntries(
    (Object.keys(overrides) as RpcMethodName[]).map((
      name,
    ) => [name, rpc[name]]),
  ) as RpcOverrides;
  Object.assign(rpc, overrides);
  return () => Object.assign(rpc, originals);
}

async function flushReact(): Promise<void> {
  await act(async () => {
    await Promise.resolve();
    await Promise.resolve();
    await Promise.resolve();
  });
}

async function renderModel(): Promise<RenderedModel> {
  let latest: ContextMenuSettingsModel | null = null;
  const host = document.createElement("div");
  document.body.append(host);
  const root: Root = createRoot(host);
  function Probe() {
    latest = useContextMenuSettings();
    return null;
  }
  await act(async () => {
    root.render(<Probe />);
    await Promise.resolve();
  });
  await flushReact();
  return {
    current: () => {
      assert(latest !== null, "the settings hook should render a model");
      return latest;
    },
    flush: flushReact,
    cleanup: () => {
      act(() => root.unmount());
      host.remove();
    },
  };
}

function createStoredRpc(initialConfig: string | null): {
  overrides: RpcOverrides;
  setEnabled(value: boolean | null, typeMismatch?: boolean): void;
  setConfig(value: string | null, typeMismatch?: boolean): void;
  setCatalog(value: ContextMenuCatalogSnapshot): void;
} {
  let enabled: boolean | null = true;
  let enabledTypeMismatch = false;
  let config = initialConfig;
  let configTypeMismatch = false;
  let catalog = EMPTY_CATALOG;
  return {
    overrides: {
      getBoolPrefState: () =>
        Promise.resolve({
          value: enabledTypeMismatch ? null : enabled,
          typeMismatch: enabledTypeMismatch,
        }),
      getStringPrefState: () =>
        Promise.resolve({
          value: configTypeMismatch ? null : config,
          typeMismatch: configTypeMismatch,
        }),
      compareAndSetBoolPref: (_name, expected, value) => {
        if (enabledTypeMismatch) {
          return Promise.resolve({
            updated: false,
            currentValue: null,
            typeMismatch: true,
          });
        }
        if (enabled !== expected) {
          return Promise.resolve({ updated: false, currentValue: enabled });
        }
        enabled = value;
        return Promise.resolve({ updated: true, currentValue: value });
      },
      compareAndSetStringPref: (_name, expected, value) => {
        if (configTypeMismatch) {
          return Promise.resolve({
            updated: false,
            currentValue: null,
            typeMismatch: true,
          });
        }
        if (config !== expected) {
          return Promise.resolve({ updated: false, currentValue: config });
        }
        config = value;
        return Promise.resolve({ updated: true, currentValue: value });
      },
      getContextMenuCatalog: () => Promise.resolve(catalog),
      getContextMenuCatalogRevision: () => Promise.resolve(catalog.revision),
    },
    setEnabled: (value, typeMismatch = false) => {
      enabled = value;
      enabledTypeMismatch = typeMismatch;
    },
    setConfig: (value, typeMismatch = false) => {
      config = value;
      configTypeMismatch = typeMismatch;
    },
    setCatalog: (value) => {
      catalog = value;
    },
  };
}

async function testFocusRefreshesCatalogWithoutRevisionRegression(): Promise<
  void
> {
  const stored = createStoredRpc(null);
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assertEquals(
      rendered.current().catalog?.revision,
      0,
      "initial catalog snapshot is exposed",
    );

    stored.setCatalog(createCatalog(2, "new-item"));
    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().catalog?.surfaces[0]?.profiles[0]?.containers[0]
        ?.items[0]?.key,
      "new-item",
      "focus adopts a catalog reported after the Hub first loaded",
    );

    stored.setCatalog(createCatalog(1, "stale-item"));
    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().catalog?.revision,
      2,
      "a stale response cannot regress the accepted catalog revision",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testFocusQueuesCatalogRefreshDuringInflightRequest(): Promise<
  void
> {
  const stored = createStoredRpc(null);
  let resolveInitial: ((catalog: ContextMenuCatalogSnapshot) => void) | null =
    null;
  let catalogCalls = 0;
  stored.overrides.getContextMenuCatalog = () => {
    catalogCalls++;
    if (catalogCalls === 1) {
      return new Promise((resolve) => {
        resolveInitial = resolve;
      });
    }
    return Promise.resolve(createCatalog(2, "queued-item"));
  };
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    globalThis.dispatchEvent(new Event("focus"));
    await act(async () => {
      assert(resolveInitial !== null, "the initial catalog request is pending");
      resolveInitial(EMPTY_CATALOG);
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();

    assertEquals(
      catalogCalls,
      2,
      "focus queues one fresh snapshot after the in-flight request",
    );
    assertEquals(
      rendered.current().catalog?.surfaces[0]?.profiles[0]?.containers[0]
        ?.items[0]?.key,
      "queued-item",
      "the queued request observes a report that the older request missed",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testSlowEmptyInitialRequestRetriesAfterCompletion(): Promise<
  void
> {
  const retryTimer = captureTimeout(250);
  const stored = createStoredRpc(null);
  let resolveInitial: ((catalog: ContextMenuCatalogSnapshot) => void) | null =
    null;
  let catalogCalls = 0;
  stored.overrides.getContextMenuCatalog = () => {
    catalogCalls++;
    if (catalogCalls === 1) {
      return new Promise((resolve) => {
        resolveInitial = resolve;
      });
    }
    return Promise.resolve(createCatalog(1, "startup-item"));
  };
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assertEquals(
      catalogCalls,
      1,
      "startup retries wait for the slow request instead of coalescing into it",
    );
    assert(
      !retryTimer.pending(),
      "no retry timer starts before the slow request completes",
    );

    await act(async () => {
      assert(resolveInitial !== null, "the slow initial request is pending");
      resolveInitial(EMPTY_CATALOG);
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();
    assert(
      retryTimer.pending(),
      "an empty result schedules its retry after completion",
    );
    await act(async () => {
      retryTimer.run();
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();
    assertEquals(
      catalogCalls,
      2,
      "an empty result schedules a retry relative to request completion",
    );
    assertEquals(
      rendered.current().catalog?.surfaces[0]?.profiles[0]?.containers[0]
        ?.items[0]?.key,
      "startup-item",
      "the completion-relative retry adopts the initialized browser catalog",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
    retryTimer.restore();
  }
}

async function testUnmountCancelsCatalogRetry(): Promise<void> {
  const retryTimer = captureTimeout(250);
  const stored = createStoredRpc(null);
  let catalogCalls = 0;
  stored.overrides.getContextMenuCatalog = () => {
    catalogCalls++;
    return Promise.resolve(EMPTY_CATALOG);
  };
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  let cleaned = false;
  try {
    assert(retryTimer.pending(), "the empty catalog schedules a startup retry");
    rendered.cleanup();
    cleaned = true;
    assert(!retryTimer.pending(), "unmount clears the captured retry timer");
    assertEquals(
      catalogCalls,
      1,
      "unmount clears the pending startup retry timer",
    );
  } finally {
    if (!cleaned) rendered.cleanup();
    restoreRpc();
    retryTimer.restore();
  }
}

async function testRevisionPollingRefreshesSameWindowPopup(): Promise<void> {
  const revisionPoll = captureInterval(1_500);
  const stored = createStoredRpc(null);
  stored.setCatalog(createCatalog(1, "initial-item"));
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assert(revisionPoll.pending(), "the visible Hub probes catalog revisions");
    stored.setCatalog(createCatalog(2, "popup-item"));
    await act(async () => {
      revisionPoll.run();
      await Promise.resolve();
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();
    assertEquals(
      rendered.current().catalog?.surfaces[0]?.profiles[0]?.containers[0]
        ?.items[0]?.key,
      "popup-item",
      "a same-window popup report is adopted without focus or visibility changes",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
    revisionPoll.restore();
  }
}

async function testRevisionPollingRecoversAfterProbeFailure(): Promise<void> {
  const revisionPoll = captureInterval(1_500);
  const stored = createStoredRpc(null);
  stored.setCatalog(createCatalog(1, "initial-item"));
  let revisionCalls = 0;
  stored.overrides.getContextMenuCatalogRevision = () => {
    revisionCalls++;
    if (revisionCalls === 1) {
      return Promise.reject(new Error("temporary bridge failure"));
    }
    return Promise.resolve(2);
  };
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    stored.setCatalog(createCatalog(2, "recovered-item"));
    await act(async () => {
      revisionPoll.run();
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();
    assertEquals(
      rendered.current().catalog?.revision,
      1,
      "a failed lightweight probe leaves the accepted catalog unchanged",
    );

    await act(async () => {
      revisionPoll.run();
      await Promise.resolve();
      await Promise.resolve();
      await Promise.resolve();
    });
    await rendered.flush();
    assertEquals(
      revisionCalls,
      2,
      "the next interval retries after a temporary revision probe failure",
    );
    assertEquals(
      rendered.current().catalog?.surfaces[0]?.profiles[0]?.containers[0]
        ?.items[0]?.key,
      "recovered-item",
      "a later successful probe refreshes the catalog",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
    revisionPoll.restore();
  }
}

async function testFocusSynchronizesExternalPreferences(): Promise<void> {
  const initial = createDefaultContextMenuConfig();
  const stored = createStoredRpc(serializeContextMenuConfig(initial));
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assertEquals(rendered.current().loading, false, "initial load completes");
    const external = setContextMenuItemHidden(
      initial,
      {
        surfaceKey: "content",
        profileKey: "link",
        containerKey: "root",
      },
      "external-item",
      true,
    );
    stored.setEnabled(false);
    stored.setConfig(serializeContextMenuConfig(external));

    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().enabled,
      false,
      "focus adopts an externally changed enabled preference",
    );
    assertEquals(
      getContextMenuLevelOverride(rendered.current().config, {
        surfaceKey: "content",
        profileKey: "link",
        containerKey: "root",
      }).hidden?.[0],
      "external-item",
      "focus adopts an externally changed configuration",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testWrongTypeRecoveryWithSameNullVersion(): Promise<void> {
  const stored = createStoredRpc(null);
  stored.setConfig(null, true);
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assertEquals(
      rendered.current().loadErrorKind,
      "read",
      "a wrong-type config is blocked during initial load",
    );

    stored.setConfig(null, false);
    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().loadErrorKind,
      null,
      "focus distinguishes a now-missing pref from the old wrong-type pref",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testConfigCasTypeMismatchRecoversWithSameNullVersion(): Promise<
  void
> {
  const stored = createStoredRpc(null);
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    stored.setConfig(null, true);
    let saved = true;
    await act(async () => {
      saved = await rendered.current().resetAll();
    });
    assertEquals(saved, false, "the wrong-type config blocks the CAS write");
    assertEquals(
      rendered.current().loadErrorKind,
      "read",
      "the CAS type mismatch blocks configuration editing",
    );

    stored.setConfig(null, false);
    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().loadErrorKind,
      null,
      "focus clears the config blocking error after the pref becomes absent again",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testEnabledCasTypeMismatchRecoversWithSameNullVersion(): Promise<
  void
> {
  const stored = createStoredRpc(null);
  stored.setEnabled(null);
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    stored.setEnabled(null, true);
    let saved = true;
    await act(async () => {
      saved = await rendered.current().toggleEnabled();
    });
    assertEquals(
      saved,
      false,
      "the wrong-type enabled pref blocks the CAS write",
    );
    assertEquals(
      rendered.current().enabledUnavailable,
      true,
      "the CAS type mismatch blocks the enabled toggle",
    );

    stored.setEnabled(null, false);
    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().loadErrorKind,
      null,
      "focus clears the enabled blocking error after the pref becomes absent again",
    );
    assertEquals(
      rendered.current().enabledUnavailable,
      false,
      "the enabled toggle becomes available after recovery",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

async function testFailedRepairRestoresSpecificErrorOnFocus(): Promise<void> {
  const stored = createStoredRpc("[]");
  stored.overrides.compareAndSetStringPref = () =>
    Promise.reject(new Error("write unavailable"));
  const restoreRpc = installRpcOverrides(stored.overrides);
  const rendered = await renderModel();
  try {
    assertEquals(
      rendered.current().loadErrorKind,
      "invalid",
      "invalid JSON starts in the repairable state",
    );
    let repairResult = true;
    await act(async () => {
      repairResult = await rendered.current().repairInvalidConfig();
    });
    assertEquals(repairResult, false, "a failed repair reports failure");
    assertEquals(
      rendered.current().loadErrorKind,
      "read",
      "the failed write temporarily reports a read/write failure",
    );

    globalThis.dispatchEvent(new Event("focus"));
    await rendered.flush();
    assertEquals(
      rendered.current().loadErrorKind,
      "invalid",
      "focus reclassifies the unchanged raw value as repairable invalid JSON",
    );
  } finally {
    rendered.cleanup();
    restoreRpc();
  }
}

const tests: TestCase[] = [
  {
    name: "focus refreshes catalog without revision regression",
    fn: testFocusRefreshesCatalogWithoutRevisionRegression,
  },
  {
    name: "focus queues catalog refresh during an in-flight request",
    fn: testFocusQueuesCatalogRefreshDuringInflightRequest,
  },
  {
    name: "slow empty initial request retries after completion",
    fn: testSlowEmptyInitialRequestRetriesAfterCompletion,
  },
  {
    name: "unmount cancels catalog retry",
    fn: testUnmountCancelsCatalogRetry,
  },
  {
    name: "revision polling refreshes a same-window popup",
    fn: testRevisionPollingRefreshesSameWindowPopup,
  },
  {
    name: "revision polling recovers after a probe failure",
    fn: testRevisionPollingRecoversAfterProbeFailure,
  },
  {
    name: "focus synchronizes external preferences",
    fn: testFocusSynchronizesExternalPreferences,
  },
  {
    name: "wrong-type recovery with the same null version",
    fn: testWrongTypeRecoveryWithSameNullVersion,
  },
  {
    name: "config CAS type mismatch recovers with the same null version",
    fn: testConfigCasTypeMismatchRecoversWithSameNullVersion,
  },
  {
    name: "enabled CAS type mismatch recovers with the same null version",
    fn: testEnabledCasTypeMismatchRecoversWithSameNullVersion,
  },
  {
    name: "failed repair restores its specific error on focus",
    fn: testFailedRepairRestoresSpecificErrorOnFocus,
  },
];

export async function runAllTests(): Promise<void> {
  const environment = globalThis as typeof globalThis & {
    IS_REACT_ACT_ENVIRONMENT?: boolean;
  };
  environment.IS_REACT_ACT_ENVIRONMENT = true;
  await runTests("ContextMenuDataManager.test.tsx", tests);
}
