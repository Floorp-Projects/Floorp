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

type RpcMethodName =
  | "getBoolPrefState"
  | "getStringPrefState"
  | "compareAndSetBoolPref"
  | "compareAndSetStringPref"
  | "getContextMenuCatalog";

type RpcOverrides = Pick<typeof rpc, RpcMethodName>;

interface RenderedModel {
  current(): ContextMenuSettingsModel;
  flush(): Promise<void>;
  cleanup(): void;
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
} {
  let enabled: boolean | null = true;
  let enabledTypeMismatch = false;
  let config = initialConfig;
  let configTypeMismatch = false;
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
      getContextMenuCatalog: () => Promise.resolve(EMPTY_CATALOG),
    },
    setEnabled: (value, typeMismatch = false) => {
      enabled = value;
      enabledTypeMismatch = typeMismatch;
    },
    setConfig: (value, typeMismatch = false) => {
      config = value;
      configTypeMismatch = typeMismatch;
    },
  };
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
