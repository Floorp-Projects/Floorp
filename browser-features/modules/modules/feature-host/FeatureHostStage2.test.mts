// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type { FeatureDefinition, PinnedFixture } from "./FeatureHostTypes.ts";
import type {
  AppMenuAcknowledgement,
  AppMenuAdapterConnection,
  AppMenuOwner,
  AppMenuRemovalReason,
} from "./AppMenuRegistry.sys.mts";

type HostModule = typeof import("./FeatureHost.sys.mts");
const {
  createFeatureHostForTests,
  M0FeatureHost,
  m0PayloadEvaluationsForTests,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as HostModule;
type RegistryModule = typeof import("./AppMenuRegistry.sys.mts");
const {
  AppMenuRegistry,
  HUB_PANEL_MENU_FEATURE_ID,
  HUB_PANEL_MENU_TRIGGER_ID,
} = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/AppMenuRegistry.sys.mjs",
  ) as RegistryModule;
type AppMenuRegistryInstance = InstanceType<typeof AppMenuRegistry>;
const { isEligibleFeatureWindow } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/WindowIdentity.sys.mjs",
) as typeof import("./WindowIdentity.sys.mts");

function assert(condition: boolean, message: string): void {
  harnessAssert(condition, message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  harnessAssertEquals(actual, expected, message);
}

async function digest(source: string): Promise<string> {
  const encoded = new TextEncoder().encode(source);
  const buffer = new ArrayBuffer(encoded.byteLength);
  new Uint8Array(buffer).set(encoded);
  const hash = await crypto.subtle.digest("SHA-256", buffer);
  return [...new Uint8Array(hash)].map((byte) =>
    byte.toString(16).padStart(2, "0")
  ).join("");
}

const MODULE_SOURCE =
  `(() => { globalThis.__floorpM0 = { activate() {}, attachWindow() {}, health() { return "ready"; }, deactivate() {} }; })();`;

async function definition(
  initialEnabled: boolean | undefined,
  version = "v1",
): Promise<FeatureDefinition> {
  const fixture: PinnedFixture<string> = {
    version: "v1",
    digest: await digest(MODULE_SOURCE),
    source: MODULE_SOURCE,
    compatibility: {
      hostSchema: 2,
      hostBuildId: "m0-schema-2",
      runtimeBuildIds: [Services.appinfo.appBuildID],
      capabilities: [],
    },
  };
  return {
    featureId: "initial-state-test",
    fixtures: { v1: fixture },
    ...(initialEnabled === undefined
      ? {}
      : { initialState: { version, enabled: initialEnabled } }),
  };
}

function rootPath(): string {
  return PathUtils.join(
    PathUtils.profileDir,
    "floorp-hotfix-m0-tests",
    `feature-${
      Services.uuid.generateUUID().toString().replace(/[{}]/g, "").toLowerCase()
    }`,
  );
}

function openTestBrowserWindow(): Window {
  const parent = Services.wm.getMostRecentWindow("navigator:browser");
  if (!parent) throw new Error("test runner browser window unavailable");
  const args = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  args.data = "about:blank";
  return Services.ww.openWindow(
    parent,
    "chrome://browser/content/browser.xhtml",
    "_blank",
    "chrome,all,dialog=no,width=640,height=480",
    args,
  ) as Window;
}

async function waitForDelayedStartup(browserWindow: Window): Promise<void> {
  if (
    (browserWindow as Window & {
      gBrowserInit?: { delayedStartupFinished?: boolean };
    }).gBrowserInit?.delayedStartupFinished
  ) return;
  await new Promise<void>((resolve, reject) => {
    let timer: number | null = null;
    const observer: nsIObserver = {
      observe: (subject: nsISupports | null) => {
        if (subject !== browserWindow) return;
        Services.obs.removeObserver(
          observer,
          "browser-delayed-startup-finished",
        );
        if (timer !== null) clearTimeout(timer);
        resolve();
      },
    };
    timer = setTimeout(() => {
      Services.obs.removeObserver(observer, "browser-delayed-startup-finished");
      reject(new Error("browser delayed startup timed out"));
    }, 30_000);
    Services.obs.addObserver(observer, "browser-delayed-startup-finished");
  });
}

async function openAppMenuTestWindow(): Promise<Window> {
  const browserWindow = openTestBrowserWindow();
  try {
    await waitForDelayedStartup(browserWindow);
    const trigger = browserWindow.document.getElementById(
      HUB_PANEL_MENU_TRIGGER_ID,
    );
    if (!isEligibleFeatureWindow(browserWindow) || !trigger?.isConnected) {
      throw new Error("opened test window is missing its app-menu trigger");
    }
    return browserWindow;
  } catch (error) {
    if (!browserWindow.closed) browserWindow.close();
    throw error;
  }
}

async function openAppMenuTestWindows(count: number): Promise<Window[]> {
  const windows: Window[] = [];
  try {
    for (let index = 0; index < count; index += 1) {
      windows.push(await openAppMenuTestWindow());
    }
    return windows;
  } catch (error) {
    for (const browserWindow of windows) {
      if (!browserWindow.closed) browserWindow.close();
    }
    throw error;
  }
}

async function waitForCondition(
  condition: () => boolean,
  label: string,
  timeoutMs = 5_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (condition()) return;
    await new Promise<void>((resolve) => setTimeout(resolve, 20));
  }
  throw new Error(`${label} timed out`);
}

async function rejectMessage(promise: Promise<unknown>): Promise<string> {
  try {
    await promise;
  } catch (error) {
    return error instanceof Error ? error.message : String(error);
  }
  return "";
}

async function runCleanupSteps(
  actions: readonly (() => unknown)[],
  message: string,
): Promise<void> {
  const errors: Error[] = [];
  for (const action of actions) {
    try {
      await action();
    } catch (error) {
      errors.push(error instanceof Error ? error : new Error(String(error)));
    }
  }
  if (errors.length) throw new AggregateError(errors, message);
}

async function testAbsentRecordGetsFrozenInitialSelection(): Promise<void> {
  const root = rootPath();
  const writes: Record<string, unknown>[] = [];
  const options = {
    excludeWindowsForTests: true,
    afterDurableWriteForTests: (record: Readonly<Record<string, unknown>>) =>
      Promise.resolve().then(() => {
        writes.push({ ...record });
      }),
  };
  let opened:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  try {
    opened = await createFeatureHostForTests(
      await definition(true),
      true,
      root,
      false,
      options,
    );
    assertEquals(
      writes[0]?.selectedVersion,
      "v1",
      "initial version is durable first",
    );
    assertEquals(
      writes[0]?.desiredEnabled,
      true,
      "initial desired state is enabled",
    );
    assertEquals(
      writes[0]?.selectedHealth,
      "unverified",
      "initial health is unverified",
    );
    const record = await IOUtils.readJSON(
      PathUtils.join(opened.root, "state.json"),
    ) as Record<string, unknown>;
    assertEquals(
      record.selectedVersion,
      "v1",
      "recovery retains initial version",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (!opened) return;
        const report = await opened.cleanup();
        assertEquals(report.errors.length, 0, "initial-state host cleans up");
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "initial-state test cleanup failed");
  }
}

async function testInitialStateInputSnapshotAndFingerprintConflict(): Promise<
  void
> {
  const root = rootPath();
  const initialState = { version: "v1", enabled: true };
  const baseDefinition = await definition(undefined);
  const callerDefinition = { ...baseDefinition, initialState };
  let opened:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  try {
    const opening = createFeatureHostForTests(
      callerDefinition,
      true,
      root,
      false,
      { excludeWindowsForTests: true },
    );
    initialState.enabled = false;
    opened = await opening;
    const record = await IOUtils.readJSON(
      PathUtils.join(root, "state.json"),
    ) as Record<string, unknown>;
    assertEquals(
      record.desiredEnabled,
      true,
      "caller mutation cannot change the frozen initial selection",
    );
    const conflict = await rejectMessage(createFeatureHostForTests(
      { ...baseDefinition, initialState: { version: "v1", enabled: false } },
      true,
      root,
      true,
      { excludeWindowsForTests: true },
    ));
    assert(
      conflict.includes("feature definition conflicts with existing root"),
      "changed initial input conflicts with the frozen definition fingerprint",
    );
  } finally {
    await runCleanupSteps([
      () => opened?.cleanup(),
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "initial-state snapshot test cleanup failed");
  }
}

async function testIncompatibleExistingRecordDoesNotReceiveDefaults(): Promise<
  void
> {
  const root = rootPath();
  const featureDefinition = await definition(true);
  let first:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  let reopened:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  try {
    first = await createFeatureHostForTests(
      featureDefinition,
      true,
      root,
      false,
      { excludeWindowsForTests: true },
    );
    await first.shutdownForRestartForTests();
    first = null;
    const statePath = PathUtils.join(root, "state.json");
    const record = await IOUtils.readJSON(statePath) as Record<string, unknown>;
    record.selectedVersion = "not-installed";
    record.selectedDigest = "0".repeat(64);
    record.desiredEnabled = false;
    await IOUtils.writeJSON(statePath, record);
    try {
      reopened = await createFeatureHostForTests(
        featureDefinition,
        true,
        root,
        true,
        { excludeWindowsForTests: true },
      );
    } catch {
      reopened = null;
    }
    const preserved = await IOUtils.readJSON(statePath) as Record<
      string,
      unknown
    >;
    assertEquals(
      preserved.selectedVersion,
      "not-installed",
      "incompatible existing selection is not replaced by defaults",
    );
    assertEquals(
      preserved.desiredEnabled,
      false,
      "incompatible existing enabled state is preserved",
    );
  } finally {
    await runCleanupSteps([
      () => reopened?.cleanup(),
      () => first?.cleanup(),
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "incompatible record test cleanup failed");
  }
}

async function testExistingDisabledRecordDoesNotReceiveDefaults(): Promise<
  void
> {
  const root = rootPath();
  const featureDefinition = await definition(true);
  let first:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  let second:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  try {
    first = await createFeatureHostForTests(
      featureDefinition,
      true,
      root,
      false,
      { excludeWindowsForTests: true },
    );
    await first.host.disable(
      "persist-disabled-state",
      first.host.diagnostics.stateRevision,
    );
    await first.shutdownForRestartForTests();
    first = null;
    second = await createFeatureHostForTests(
      featureDefinition,
      true,
      root,
      true,
      { excludeWindowsForTests: true },
    );
    const record = await IOUtils.readJSON(
      PathUtils.join(root, "state.json"),
    ) as Record<string, unknown>;
    assertEquals(
      record.desiredEnabled,
      false,
      "existing disabled state is preserved",
    );
    assertEquals(
      record.selectedVersion,
      "v1",
      "existing selection is preserved",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (!second) return;
        const report = await second.cleanup();
        assertEquals(report.errors.length, 0, "reopened host cleans up");
      },
      () => first?.cleanup(),
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "existing disabled record test cleanup failed");
  }
}

async function testEmptyQuarantinedAndInvalidRecordsDoNotGetDefaults(): Promise<
  void
> {
  const featureDefinition = await definition(true);
  for (const kind of ["empty", "quarantined"] as const) {
    const root = rootPath();
    let first:
      | Awaited<ReturnType<typeof createFeatureHostForTests>>
      | null = null;
    let reopened:
      | Awaited<ReturnType<typeof createFeatureHostForTests>>
      | null = null;
    try {
      first = await createFeatureHostForTests(
        featureDefinition,
        true,
        root,
        false,
        { excludeWindowsForTests: true },
      );
      const firstShutdown = await first.shutdownForRestartForTests();
      assertEquals(
        firstShutdown.errors.length,
        0,
        `${kind} setup host shuts down`,
      );
      first = null;
      const statePath = PathUtils.join(root, "state.json");
      const record = await IOUtils.readJSON(statePath) as Record<
        string,
        unknown
      >;
      record.selectedVersion = null;
      record.selectedDigest = null;
      record.selectedHealth = "unverified";
      record.selectedHealthBuild = null;
      record.desiredEnabled = false;
      record.rollbackVersion = null;
      record.rollbackDigest = null;
      record.rollbackHealthBuild = null;
      record.pending = null;
      record.attempt = null;
      record.blockedReason = kind === "quarantined" ? "test-quarantine" : null;
      record.recentOutcomes = [];
      record.lastOutcome = null;
      await IOUtils.writeJSON(statePath, record);
      reopened = await createFeatureHostForTests(
        featureDefinition,
        true,
        root,
        true,
        { excludeWindowsForTests: true },
      );
      const reopenedRecord = await IOUtils.readJSON(statePath) as Record<
        string,
        unknown
      >;
      assertEquals(
        reopenedRecord.selectedVersion,
        null,
        `${kind} record stays empty`,
      );
      assertEquals(
        reopenedRecord.blockedReason,
        record.blockedReason,
        `${kind} block state is preserved`,
      );
    } finally {
      await runCleanupSteps([
        async () => {
          if (!reopened) return;
          const report = await reopened.cleanup();
          assertEquals(report.errors.length, 0, `${kind} host cleans up`);
        },
        () => first?.cleanup(),
        () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
      ], `${kind} initial-state test cleanup failed`);
    }
  }

  const invalidRoot = rootPath();
  try {
    const invalidState = PathUtils.join(invalidRoot, "state.json");
    const invalidParent = PathUtils.parent(invalidRoot);
    if (!invalidParent) throw new Error("invalid fixture root has no parent");
    await IOUtils.makeDirectory(invalidParent, {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.makeDirectory(invalidRoot, {
      createAncestors: true,
      ignoreExisting: true,
    });
    await IOUtils.writeUTF8(invalidState, "{invalid");
    const error = await rejectMessage(M0FeatureHost.openForTests(
      invalidRoot,
      true,
      true,
      undefined,
      { excludeWindowsForTests: true },
      featureDefinition,
    ));
    assert(error.length > 0, "invalid existing record rejects reopen");
    assertEquals(
      await IOUtils.readUTF8(invalidState),
      "{invalid",
      "invalid state is not overwritten",
    );
  } finally {
    await runCleanupSteps([
      () =>
        IOUtils.remove(invalidRoot, { recursive: true, ignoreAbsent: true }),
    ], "invalid record test cleanup failed");
  }
}

async function testInitialDefaultWriteFaultBoundaries(): Promise<void> {
  const fixtureDefinition = await definition(true);
  const beforeRoot = rootPath();
  const afterRoot = rootPath();
  let recovered:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  try {
    const beforeError = await rejectMessage(M0FeatureHost.openForTests(
      beforeRoot,
      true,
      false,
      { mode: "before_replace", skip: 0 },
      { excludeWindowsForTests: true },
      fixtureDefinition,
    ));
    assert(beforeError.length > 0, "pre-replace fault rejects initialization");
    assertEquals(
      await IOUtils.exists(PathUtils.join(beforeRoot, "state.json")),
      false,
      "pre-replace fault leaves no default record",
    );
    const afterError = await rejectMessage(M0FeatureHost.openForTests(
      afterRoot,
      true,
      false,
      { mode: "after_replace", skip: 0 },
      { excludeWindowsForTests: true },
      fixtureDefinition,
    ));
    assert(afterError.length > 0, "post-replace fault rejects initialization");
    assertEquals(
      await IOUtils.exists(PathUtils.join(afterRoot, "state.json")),
      true,
      "post-replace fault leaves the atomic default record",
    );
    recovered = await createFeatureHostForTests(
      fixtureDefinition,
      true,
      afterRoot,
      true,
      { excludeWindowsForTests: true },
    );
    const record = await IOUtils.readJSON(
      PathUtils.join(afterRoot, "state.json"),
    ) as Record<string, unknown>;
    assertEquals(
      record.selectedVersion,
      "v1",
      "post-replace recovery uses default selection",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (!recovered) return;
        const report = await recovered.cleanup();
        assertEquals(
          report.errors.length,
          0,
          "post-replace recovery cleans up",
        );
      },
      () => IOUtils.remove(beforeRoot, { recursive: true, ignoreAbsent: true }),
      () => IOUtils.remove(afterRoot, { recursive: true, ignoreAbsent: true }),
    ], "initial-default recovery test cleanup failed");
  }
}

async function testAppMenuReadinessAndGenerationFencing(): Promise<void> {
  const browserWindow = await openAppMenuTestWindow();
  const registry = new AppMenuRegistry();
  const leases: ReturnType<AppMenuRegistryInstance["beginGeneration"]>[] = [];
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let connection: AppMenuAdapterConnection | null = null;
  let removed = 0;
  let renderAvailable = true;
  let failures = 0;
  try {
    const hostClaim = registry.claimHost(
      HUB_PANEL_MENU_FEATURE_ID,
      "stage2-test",
    );
    claim = hostClaim;
    const activeConnection = registry.registerAdapter(browserWindow, {
      render: () =>
        !renderAvailable
          ? null
          : browserWindow.document.getElementById("PanelUI-menu-button")
              ?.getAttribute("open") === "true"
          ? "rendered"
          : "deferred",
      setEnabled: () => {},
      remove: () => removed++,
    });
    connection = activeConnection;
    const lease = registry.beginGeneration({
      featureId: HUB_PANEL_MENU_FEATURE_ID,
      hostEpoch: "stage2-test",
      generation: 1,
    }, () => {
      failures += 1;
    }, () => true);
    leases.push(lease);
    registry.stageDescriptor(lease, { command: "open-hub" });
    const signal = new AbortController().signal;
    await registry.stageAndWait(lease, [browserWindow], signal, 500);
    assertEquals(
      activeConnection.invoke(lease.owner),
      false,
      "staged commands remain disabled",
    );
    await registry.publishAndWait(lease, [browserWindow], signal, 500);
    registry.prepareEnable(lease);
    registry.enable(lease);
    assertEquals(
      registry.diagnosticsForTests.enabled,
      true,
      "generation enables after readiness",
    );
    renderAvailable = false;
    activeConnection.notifyReady();
    assertEquals(
      failures,
      0,
      "temporarily unavailable published render starts a bounded wait",
    );
    assertEquals(
      activeConnection.invoke(lease.owner),
      false,
      "unacknowledged window cannot invoke the native command",
    );
    assertEquals(
      activeConnection.reportFailure(
        lease.owner,
        new Error("translation failed during readiness wait"),
      ),
      true,
      "the delivered owner may fail between readiness renders",
    );
    assertEquals(failures, 1, "between-render failure reaches host policy");
    renderAvailable = true;
    activeConnection.notifyReady();
    assertEquals(failures, 1, "apparent recovery cannot clear sticky failure");
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "failure settles the readiness deadline",
    );
    assertEquals(
      activeConnection.invoke(lease.owner),
      false,
      "sticky between-render failure keeps commands disabled",
    );
    registry.revoke(lease);
    assertEquals(
      activeConnection.invoke(lease.owner),
      false,
      "revoked handler is stale",
    );
    renderAvailable = true;
    const replacementLease = registry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "stage2-test",
        generation: 2,
      },
      () => {},
      () => true,
    );
    leases.push(replacementLease);
    registry.stageDescriptor(replacementLease, { command: "open-hub" });
    await registry.stageAndWait(replacementLease, [browserWindow], signal, 500);
    await registry.publishAndWait(
      replacementLease,
      [browserWindow],
      signal,
      500,
    );
    registry.prepareEnable(replacementLease);
    registry.enable(replacementLease);
    assertEquals(
      activeConnection.invoke(lease.owner),
      false,
      "replaced generation handler is stale",
    );
    registry.revoke(replacementLease);
    activeConnection.dispose();
    hostClaim.release();
    assert(removed > 0, "generation cleanup removes its owned UI");
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "readiness waiters are cleared",
    );
  } finally {
    await runCleanupSteps([
      ...leases.map((lease) => () => {
        const errors = registry.revoke(lease);
        if (errors.length) {
          throw new AggregateError(errors, "readiness test revoke failed");
        }
      }),
      () => connection?.dispose(),
      () => claim?.release(),
      () => {
        if (!browserWindow.closed) browserWindow.close();
      },
    ], "readiness test cleanup failed");
  }
}

async function testReentrantAdapterReplacementFencesOldAcknowledgement(): Promise<
  void
> {
  const browserWindow = await openAppMenuTestWindow();
  const registry = new AppMenuRegistry();
  const signal = new AbortController().signal;
  let failureCount = 0;
  let replacement: AppMenuAdapterConnection | null = null;
  let didReplace = false;
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let oldConnection: AppMenuAdapterConnection | null = null;
  let lease: ReturnType<AppMenuRegistryInstance["beginGeneration"]> | null =
    null;
  try {
    claim = registry.claimHost(HUB_PANEL_MENU_FEATURE_ID, "reentrant-test");
    const initialConnection = registry.registerAdapter(browserWindow, {
      render: () => {
        if (!didReplace) {
          didReplace = true;
          replacement = registry.registerAdapter(browserWindow, {
            render: () => "deferred",
            setEnabled: () => {},
            remove: () => {},
          });
        }
        return "invalid" as unknown as AppMenuAcknowledgement;
      },
      setEnabled: () => {},
      remove: () => {},
    });
    oldConnection = initialConnection;
    lease = registry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "reentrant-test",
        generation: 1,
      },
      () => failureCount++,
      () => true,
    );
    registry.stageDescriptor(lease, { command: "open-hub" });
    await registry.stageAndWait(lease, [browserWindow], signal, 500);
    harnessAssert(didReplace, "render reentrantly installed a replacement");
    harnessAssert(replacement, "replacement connection is current");
    harnessAssertEquals(failureCount, 0, "stale acknowledgement is ignored");
  } finally {
    await runCleanupSteps([
      () => {
        if (!lease) return;
        const errors = registry.revoke(lease);
        if (errors.length) {
          throw new AggregateError(errors, "reentrant test revoke failed");
        }
      },
      () => replacement?.dispose(),
      () => oldConnection?.dispose(),
      () => claim?.release(),
      () => {
        if (!browserWindow.closed) browserWindow.close();
      },
    ], "reentrant adapter test cleanup failed");
  }
}

async function testMissingAdapterAndHmrRebindAreBounded(): Promise<void> {
  const missingWindow = await openAppMenuTestWindow();
  const missingRegistry = new AppMenuRegistry();
  let missingClaim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null =
    null;
  let missingLease:
    | ReturnType<AppMenuRegistryInstance["beginGeneration"]>
    | null = null;
  try {
    missingClaim = missingRegistry.claimHost(
      HUB_PANEL_MENU_FEATURE_ID,
      "stage2-missing",
    );
    missingLease = missingRegistry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "stage2-missing",
        generation: 1,
      },
      () => {},
      () => true,
    );
    missingRegistry.stageDescriptor(missingLease, { command: "open-hub" });
    const missingError = await rejectMessage(missingRegistry.stageAndWait(
      missingLease,
      [missingWindow],
      new AbortController().signal,
      5,
    ));
    assert(
      missingError.includes("timed out"),
      "missing live adapter cannot acknowledge",
    );
  } finally {
    await runCleanupSteps([
      () => {
        if (!missingLease) return;
        const errors = missingRegistry.revoke(missingLease);
        if (errors.length) {
          throw new AggregateError(errors, "missing adapter revoke failed");
        }
      },
      () => missingClaim?.release(),
      () => {
        if (!missingWindow.closed) missingWindow.close();
      },
    ], "missing adapter test cleanup failed");
  }

  const browserWindow = await openAppMenuTestWindow();
  const registry = new AppMenuRegistry();
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let first: AppMenuAdapterConnection | null = null;
  let replacement: AppMenuAdapterConnection | null = null;
  let rebound: AppMenuAdapterConnection | null = null;
  let lease: ReturnType<AppMenuRegistryInstance["beginGeneration"]> | null =
    null;
  try {
    const hostClaim = registry.claimHost(
      HUB_PANEL_MENU_FEATURE_ID,
      "stage2-hmr",
    );
    claim = hostClaim;
    let renderAvailable = true;
    const initialConnection = registry.registerAdapter(browserWindow, {
      render: () => null,
      setEnabled: () => {},
      remove: () => {},
    });
    first = initialConnection;
    const activeLease = registry.beginGeneration({
      featureId: HUB_PANEL_MENU_FEATURE_ID,
      hostEpoch: "stage2-hmr",
      generation: 1,
    }, () => {
      throw new Error("unexpected late HMR failure");
    }, () => true);
    lease = activeLease;
    registry.stageDescriptor(activeLease, { command: "open-hub" });
    const readiness = registry.stageAndWait(
      activeLease,
      [browserWindow],
      new AbortController().signal,
      500,
    );
    initialConnection.dispose();
    const nextConnection = registry.registerAdapter(browserWindow, {
      render: () =>
        !renderAvailable
          ? null
          : browserWindow.document.getElementById("PanelUI-menu-button")
              ?.getAttribute("open") === "true"
          ? "rendered"
          : "deferred",
      setEnabled: () => {},
      remove: () => {},
    });
    replacement = nextConnection;
    await readiness;
    assertEquals(
      initialConnection.reportFailure(
        activeLease.owner,
        new Error("stale HMR failure"),
      ),
      false,
      "disposed HMR connection cannot report a failure",
    );
    assertEquals(
      nextConnection.reportFailure(
        { ...activeLease.owner, generation: activeLease.owner.generation + 1 },
        new Error("stale owner failure"),
      ),
      false,
      "current adapter cannot report for a stale generation owner",
    );
    await registry.publishAndWait(
      activeLease,
      [browserWindow],
      new AbortController().signal,
      500,
    );
    registry.prepareEnable(activeLease);
    registry.enable(activeLease);
    renderAvailable = false;
    nextConnection.notifyReady();
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      1,
      "published missing render starts one readiness waiter",
    );
    nextConnection.dispose();
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      1,
      "HMR disposal does not cancel or complete the readiness deadline",
    );
    const reboundConnection = registry.registerAdapter(browserWindow, {
      render: () =>
        browserWindow.document.getElementById("PanelUI-menu-button")
            ?.getAttribute("open") === "true"
          ? "rendered"
          : "deferred",
      setEnabled: () => {},
      remove: () => {},
    });
    rebound = reboundConnection;
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      0,
      "HMR rebind satisfies the original published deadline",
    );
    registry.revoke(activeLease);
    reboundConnection.dispose();
    hostClaim.release();
    assertEquals(
      registry.diagnosticsForTests.connections,
      0,
      "HMR connections clean up",
    );
  } finally {
    await runCleanupSteps([
      () => {
        if (!lease) return;
        const errors = registry.revoke(lease);
        if (errors.length) {
          throw new AggregateError(errors, "HMR test revoke failed");
        }
      },
      () => rebound?.dispose(),
      () => replacement?.dispose(),
      () => first?.dispose(),
      () => claim?.release(),
      () => {
        if (!browserWindow.closed) browserWindow.close();
      },
    ], "HMR rebind test cleanup failed");
  }
}

async function testInFlightPublicationFailureSettlesOnce(): Promise<void> {
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const testWindow = await openAppMenuTestWindow();
  const windows = [testWindow];
  const connections: AppMenuAdapterConnection[] = [];
  const renderCounts = new Map<Window, number>();
  const durablePending: Record<string, unknown>[] = [];
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: () => {
          const count = (renderCounts.get(browserWindow) ?? 0) + 1;
          renderCounts.set(browserWindow, count);
          if (count === 2) throw new Error("publication renderer failed");
          return "deferred";
        },
        setEnabled: () => {},
        remove: () => {},
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
        afterDurableWriteForTests: (record) => {
          if (
            record.lastOutcome &&
            typeof record.lastOutcome === "object" &&
            "status" in record.lastOutcome &&
            record.lastOutcome.status === "committed_pending_publish"
          ) durablePending.push({ ...record });
          return Promise.resolve();
        },
      },
    );
    const record = host.diagnostics;
    const outcome = record.lastOutcome;
    assertEquals(
      durablePending.length,
      1,
      "publication failure occurs after the durable pending write",
    );
    assertEquals(record.pending, null, "failed publication clears pending");
    assertEquals(
      outcome?.status,
      "failed_quarantined",
      "in-flight renderer failure terminalizes the activation",
    );
    assertEquals(
      record.recentOutcomes.filter((entry) => entry.txId === outcome?.txId)
        .length,
      0,
      "the activation has only one terminal record",
    );
    assertEquals(
      JSON.stringify(host.retryExisting(outcome!.txId)),
      JSON.stringify(outcome),
      "terminal result remains replayable after publication failure",
    );
    await new Promise<void>((resolve) => setTimeout(resolve, 30));
    const persisted = await IOUtils.readJSON(
      PathUtils.join(root, "state.json"),
    ) as Record<string, unknown>;
    assertEquals(persisted.pending, null, "no deferred failure leaves pending");
    assertEquals(
      JSON.stringify(persisted.lastOutcome),
      JSON.stringify(outcome),
      "no second quarantine overwrites the terminal result",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (host) await host.shutdown();
      },
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        if (!testWindow.closed) testWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "in-flight publication test cleanup failed");
  }
}

async function testLateMenuFailurePreservesTerminalReplay(): Promise<void> {
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const testWindow = await openAppMenuTestWindow();
  const windows = [testWindow];
  const connections: AppMenuAdapterConnection[] = [];
  const owners = new Map<Window, AppMenuOwner>();
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: (request) => {
          owners.set(browserWindow, request.owner);
          return "deferred";
        },
        setEnabled: () => {},
        remove: () => {},
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
      },
    );
    const revision = host.diagnostics.stateRevision;
    const txId = "stage2-terminal-health";
    const terminal = await host.update(txId, revision, "hub-menu-v2");
    assertEquals(terminal.status, "succeeded", "feature update completes");
    const connection = connections[0];
    const owner = owners.get(windows[0]!);
    if (!connection || !owner) throw new Error("published adapter is missing");
    assertEquals(
      connection.reportFailure(
        { ...owner, generation: owner.generation + 1 },
        new Error("stale generation translation failure"),
      ),
      false,
      "stale translation failure is ignored",
    );
    assertEquals(
      connection.reportFailure(owner, new Error("late translation failure")),
      true,
      "current translation failure reaches generation policy",
    );
    assertEquals(
      registry.diagnosticsForTests.enabled,
      false,
      "late failure disables the core command gate immediately",
    );
    assertEquals(
      connection.invoke(owner),
      false,
      "late failure prevents invoking the command",
    );
    await waitForCondition(
      () => Boolean(host && host.diagnostics.blockedReason !== null),
      "late generation health block",
    );
    const blocked = host.diagnostics;
    assertEquals(
      blocked.pending,
      null,
      "late failure has no pending operation",
    );
    assertEquals(
      JSON.stringify(blocked.lastOutcome),
      JSON.stringify(terminal),
      "late quarantine preserves the terminal operation result",
    );
    const replay = await host.update(txId, revision, "hub-menu-v2");
    assertEquals(
      JSON.stringify(replay),
      JSON.stringify(terminal),
      "terminal operation replays unchanged after health quarantine",
    );
    assertEquals(
      host.diagnostics.stateRevision,
      blocked.stateRevision,
      "terminal replay performs no durable write",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (host) await host.shutdown();
      },
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        if (!testWindow.closed) testWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "late failure test cleanup failed");
  }
}

async function testTerminalWriteObserverFailurePreservesOutcome(): Promise<
  void
> {
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const testWindow = await openAppMenuTestWindow();
  const windows = [testWindow];
  const connections: AppMenuAdapterConnection[] = [];
  const owners = new Map<Window, AppMenuOwner>();
  const txId = "stage2-terminal-write-health";
  let injected = false;
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: (request) => {
          owners.set(browserWindow, request.owner);
          return "deferred";
        },
        setEnabled: () => {},
        remove: () => {},
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
        afterDurableWriteForTests: (record) => {
          const outcome = record.lastOutcome;
          if (
            !injected && typeof outcome === "object" && outcome !== null &&
            "txId" in outcome && outcome.txId === txId &&
            "status" in outcome && outcome.status === "succeeded"
          ) {
            const connection = connections[0];
            const owner = owners.get(windows[0]!);
            if (!connection || !owner) {
              throw new Error("terminal-success adapter owner is missing");
            }
            injected = connection.reportFailure(
              owner,
              new Error("failure during terminal durable observer"),
            );
          }
          return Promise.resolve();
        },
      },
    );
    const revision = host.diagnostics.stateRevision;
    const terminal = await host.update(txId, revision, "hub-menu-v2");
    assertEquals(
      terminal.status,
      "succeeded",
      "terminal write stays successful",
    );
    assert(injected, "failure was reported from the durable-write observer");
    await waitForCondition(
      () => host?.diagnostics.blockedReason !== null,
      "terminal-write health quarantine",
    );
    assertEquals(
      JSON.stringify(host.diagnostics.lastOutcome),
      JSON.stringify(terminal),
      "in-flight health failure preserves terminal outcome",
    );
    const replay = await host.update(txId, revision, "hub-menu-v2");
    assertEquals(
      JSON.stringify(replay),
      JSON.stringify(terminal),
      "terminal replay remains unchanged after observer failure",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (host) await host.shutdown();
      },
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        if (!testWindow.closed) testWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "terminal durable-write test cleanup failed");
  }
}

async function testLateReadinessTimeoutBlocksPublishedHost(): Promise<void> {
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const testWindow = await openAppMenuTestWindow();
  const windows = [testWindow];
  const connections: AppMenuAdapterConnection[] = [];
  let renderAvailable = true;
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: () => renderAvailable ? "deferred" : null,
        setEnabled: () => {},
        remove: () => {},
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
      },
    );
    const revision = host.diagnostics.stateRevision;
    const terminal = await host.update(
      "stage2-readiness-timeout",
      revision,
      "hub-menu-v2",
    );
    assertEquals(terminal.status, "succeeded", "initial publication succeeds");
    const connection = connections[0];
    if (!connection) throw new Error("published adapter is missing");
    renderAvailable = false;
    const startedAt = Date.now();
    connection.notifyReady();
    await new Promise<void>((resolve) => setTimeout(resolve, 1_400));
    connection.notifyReady();
    await waitForCondition(
      () => host?.diagnostics.blockedReason !== null,
      "persistent readiness absence blocks the host",
      1_300,
    );
    const elapsed = Date.now() - startedAt;
    assert(
      elapsed < 2_800,
      "repeated readiness notification does not reset deadline",
    );
    const blocked = host.diagnostics;
    assert(
      blocked.blockedReason?.includes(
        "app-menu adapter readiness timed out",
      ) ===
        true,
      "timeout error reaches host health policy",
    );
    assertEquals(
      registry.diagnosticsForTests.enabled,
      false,
      "timed-out readiness leaves app-menu commands disabled",
    );
    assertEquals(
      blocked.pending,
      null,
      "readiness timeout leaves no pending host operation",
    );
    assertEquals(
      JSON.stringify(blocked.lastOutcome),
      JSON.stringify(terminal),
      "late timeout preserves the terminal operation result",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (host) await host.shutdown();
      },
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        if (!testWindow.closed) testWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "late readiness test cleanup failed");
  }
}

async function testWindowClosureCancelsPublishedReadinessWait(): Promise<void> {
  let browserWindow: Window | null = null;
  let registry: AppMenuRegistryInstance | null = null;
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let connection: AppMenuAdapterConnection | null = null;
  let lease: ReturnType<AppMenuRegistryInstance["beginGeneration"]> | null =
    null;
  let renderAvailable = true;
  let failures = 0;
  try {
    const testWindow = openTestBrowserWindow();
    browserWindow = testWindow;
    await waitForDelayedStartup(testWindow);
    const activeRegistry = new AppMenuRegistry();
    registry = activeRegistry;
    const activeClaim = activeRegistry.claimHost(
      HUB_PANEL_MENU_FEATURE_ID,
      "stage2-close-wait",
    );
    claim = activeClaim;
    connection = activeRegistry.registerAdapter(testWindow, {
      render: () => renderAvailable ? "deferred" : null,
      setEnabled: () => {},
      remove: () => {},
    });
    const activeLease = activeRegistry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "stage2-close-wait",
        generation: 1,
      },
      () => failures++,
      () => true,
    );
    lease = activeLease;
    activeRegistry.stageDescriptor(activeLease, { command: "open-hub" });
    const signal = new AbortController().signal;
    await activeRegistry.stageAndWait(
      activeLease,
      [testWindow],
      signal,
      1_000,
    );
    await activeRegistry.publishAndWait(
      activeLease,
      [testWindow],
      signal,
      1_000,
    );
    activeRegistry.prepareEnable(activeLease);
    activeRegistry.enable(activeLease);
    renderAvailable = false;
    connection.notifyReady();
    assertEquals(
      activeRegistry.diagnosticsForTests.pendingAcknowledgements,
      1,
      "unavailable published view creates one bounded waiter",
    );
    testWindow.addEventListener(
      "unload",
      () => activeRegistry.closeWindow(activeLease, testWindow),
      { once: true },
    );
    testWindow.close();
    await waitForCondition(
      () => activeRegistry.diagnosticsForTests.pendingAcknowledgements === 0,
      "closed window cancels its readiness wait",
    );
    assertEquals(failures, 0, "true closure does not quarantine the lease");
  } finally {
    await runCleanupSteps([
      () => {
        if (!registry || !lease) return;
        const errors = registry.revoke(lease);
        if (errors.length) {
          throw new AggregateError(errors, "close wait revoke failed");
        }
      },
      () => connection?.dispose(),
      () => claim?.release(),
      () => {
        if (browserWindow && !browserWindow.closed) browserWindow.close();
      },
    ], "window closure test cleanup failed");
  }
}

async function testGenericHostDoesNotCloseAppMenuConnection(): Promise<void> {
  const root = rootPath();
  const registry = new AppMenuRegistry();
  let browserWindow: Window | null = null;
  let host:
    | Awaited<ReturnType<typeof createFeatureHostForTests>>
    | null = null;
  let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
  let connection: AppMenuAdapterConnection | null = null;
  let lease: ReturnType<AppMenuRegistryInstance["beginGeneration"]> | null =
    null;
  try {
    const testWindow = openTestBrowserWindow();
    browserWindow = testWindow;
    await waitForDelayedStartup(testWindow);
    host = await createFeatureHostForTests(
      await definition(true),
      true,
      root,
      false,
      {
        excludeWindowsForTests: true,
        appMenuRegistryForTests: registry,
      },
    );
    claim = registry.claimHost(HUB_PANEL_MENU_FEATURE_ID, "generic-close-test");
    connection = registry.registerAdapter(testWindow, {
      render: () => "deferred",
      setEnabled: () => {},
      remove: () => {},
    });
    lease = registry.beginGeneration(
      {
        featureId: HUB_PANEL_MENU_FEATURE_ID,
        hostEpoch: "generic-close-test",
        generation: 1,
      },
      () => {},
      () => true,
    );
    registry.stageDescriptor(lease, { command: "open-hub" });
    const signal = new AbortController().signal;
    await registry.stageAndWait(lease, [testWindow], signal, 500);
    await registry.publishAndWait(lease, [testWindow], signal, 500);
    registry.prepareEnable(lease);
    registry.enable(lease);
    testWindow.close();
    await waitForCondition(
      () => testWindow.closed,
      "test browser window close",
    );
    await new Promise<void>((resolve) => setTimeout(resolve, 20));
    assertEquals(
      registry.diagnosticsForTests.connections,
      1,
      "unrelated M0 host does not close the HUB app-menu connection",
    );
  } finally {
    await runCleanupSteps([
      async () => {
        if (!host) return;
        const report = await host.cleanup();
        if (report.errors.length) {
          throw new AggregateError(
            report.errors,
            "generic host cleanup failed",
          );
        }
      },
      () => {
        if (!lease) return;
        const errors = registry.revoke(lease);
        if (errors.length) {
          throw new AggregateError(errors, "generic close test revoke failed");
        }
      },
      () => connection?.dispose(),
      () => claim?.release(),
      () => {
        if (browserWindow && !browserWindow.closed) browserWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "generic host test cleanup failed");
  }
}

async function testMenuCleanupErrorsBlockReplacement(): Promise<void> {
  let windows: Window[] = [];
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const connections: AppMenuAdapterConnection[] = [];
  const removalCounts = new Map<Window, number>();
  let throwOnFirstRemoval = false;
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    windows = await openAppMenuTestWindows(2);
    const firstWindow = windows[0]!;
    const secondWindow = windows[1]!;
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: () => "deferred",
        setEnabled: () => {},
        remove: () => {
          removalCounts.set(
            browserWindow,
            (removalCounts.get(browserWindow) ?? 0) + 1,
          );
          if (throwOnFirstRemoval && browserWindow === firstWindow) {
            throw new Error("old menu cleanup failed");
          }
        },
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
      },
    );
    const evaluationsBefore = m0PayloadEvaluationsForTests();
    throwOnFirstRemoval = true;
    const outcome = await host.update(
      "stage2-cleanup-failure",
      host.diagnostics.stateRevision,
      "hub-menu-v2",
    );
    assertEquals(
      outcome.status,
      "failed_quarantined",
      "unclean app-menu revoke quarantines replacement",
    );
    assertEquals(
      m0PayloadEvaluationsForTests(),
      evaluationsBefore,
      "new generation is not evaluated after unclean old cleanup",
    );
    assertEquals(
      removalCounts.get(firstWindow),
      1,
      "throwing adapter cleanup is attempted exactly once",
    );
    assertEquals(
      removalCounts.get(secondWindow),
      1,
      "other adapter cleanup runs after the first throws",
    );
  } finally {
    await runCleanupSteps([
      () => host?.shutdown(),
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        for (const browserWindow of windows) {
          if (!browserWindow.closed) browserWindow.close();
        }
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "menu cleanup replacement test cleanup failed");
  }
}

async function testReplacementAndDisableRemovalReasons(): Promise<void> {
  let testWindow: Window | null = null;
  const root = rootPath();
  const registry = new AppMenuRegistry();
  const connections: AppMenuAdapterConnection[] = [];
  const removalReasons: AppMenuRemovalReason[] = [];
  let host:
    | Awaited<ReturnType<typeof M0FeatureHost.openHubPanelMenuForTests>>
    | null = null;
  try {
    testWindow = await openAppMenuTestWindow();
    const windows = [testWindow];
    for (const browserWindow of windows) {
      connections.push(registry.registerAdapter(browserWindow, {
        render: () => "deferred",
        setEnabled: () => {},
        remove: (_request, reason) => removalReasons.push(reason),
      }));
    }
    host = await M0FeatureHost.openHubPanelMenuForTests(
      root,
      false,
      undefined,
      {
        appMenuRegistryForTests: registry,
        onlyWindowsForTests: windows,
        appMenuPublicationTimeoutMsForTests: 1_000,
      },
    );
    const beforeReplacement = removalReasons.length;
    const replacement = await host.update(
      "stage2-removal-reason-replacement",
      host.diagnostics.stateRevision,
      "hub-menu-v2",
    );
    assertEquals(
      replacement.status,
      "succeeded",
      "replacement update succeeds",
    );
    const replacementReasons = removalReasons.slice(beforeReplacement);
    assert(replacementReasons.length > 0, "replacement removes old adapters");
    assert(
      replacementReasons.every((reason) => reason === "replacement"),
      "old runtime cleanup uses replacement reason",
    );

    const beforeDisable = removalReasons.length;
    const disabled = await host.disable(
      "stage2-removal-reason-disable",
      host.diagnostics.stateRevision,
    );
    assertEquals(disabled.status, "succeeded", "disable succeeds");
    const disableReasons = removalReasons.slice(beforeDisable);
    assert(disableReasons.length > 0, "disable removes active adapters");
    assert(
      disableReasons.every((reason) => reason === "remove"),
      "disable cleanup uses ordinary removal reason",
    );
  } finally {
    await runCleanupSteps([
      () => host?.shutdown(),
      ...connections.map((connection) => () => connection.dispose()),
      () => {
        if (testWindow && !testWindow.closed) testWindow.close();
      },
      () => IOUtils.remove(root, { recursive: true, ignoreAbsent: true }),
    ], "removal reason test cleanup failed");
  }
}

async function testDisconnectAndCloseCleanupFailuresAreSticky(): Promise<void> {
  const signal = new AbortController().signal;
  for (const cleanupMode of ["disconnect", "close"] as const) {
    const browserWindow = await openAppMenuTestWindow();
    const registry = new AppMenuRegistry();
    let claim: ReturnType<AppMenuRegistryInstance["claimHost"]> | null = null;
    let connection: AppMenuAdapterConnection | null = null;
    let lease: ReturnType<AppMenuRegistryInstance["beginGeneration"]> | null =
      null;
    let failures = 0;
    try {
      claim = registry.claimHost(HUB_PANEL_MENU_FEATURE_ID, cleanupMode);
      const activeConnection = registry.registerAdapter(browserWindow, {
        render: () => "deferred",
        setEnabled: () => {},
        remove: () => {
          throw new Error(`${cleanupMode} cleanup failed`);
        },
      });
      connection = activeConnection;
      const activeLease = registry.beginGeneration(
        {
          featureId: HUB_PANEL_MENU_FEATURE_ID,
          hostEpoch: cleanupMode,
          generation: 1,
        },
        () => failures++,
        () => true,
      );
      lease = activeLease;
      registry.stageDescriptor(activeLease, { command: "open-hub" });
      await registry.stageAndWait(activeLease, [browserWindow], signal, 500);
      await registry.publishAndWait(activeLease, [browserWindow], signal, 500);
      registry.prepareEnable(activeLease);
      registry.enable(activeLease);
      let rejected = false;
      try {
        if (cleanupMode === "disconnect") activeConnection.dispose();
        else registry.closeWindow(activeLease, browserWindow);
      } catch {
        rejected = true;
      }
      assert(rejected, `${cleanupMode} cleanup failure propagates`);
      assertEquals(
        failures,
        1,
        `${cleanupMode} cleanup failure reaches host policy`,
      );
      assertEquals(
        registry.diagnosticsForTests.connections,
        0,
        `${cleanupMode} cleanup forgets the connection despite failure`,
      );
      assertEquals(
        activeConnection.invoke(activeLease.owner),
        false,
        `${cleanupMode} cleanup leaves commands disabled`,
      );
    } finally {
      await runCleanupSteps([
        () => {
          if (!lease) return;
          const errors = registry.revoke(lease);
          if (errors.length) {
            throw new AggregateError(
              errors,
              `${cleanupMode} revoke cleanup failed`,
            );
          }
        },
        () => connection?.dispose(),
        () => claim?.release(),
        () => {
          if (!browserWindow.closed) browserWindow.close();
        },
      ], `${cleanupMode} test cleanup failed`);
    }
  }
}

const tests: TestCase[] = [
  {
    name: "absent records use frozen default selection",
    fn: testAbsentRecordGetsFrozenInitialSelection,
  },
  {
    name: "initial state input is snapshotted and fingerprinted",
    fn: testInitialStateInputSnapshotAndFingerprintConflict,
  },
  {
    name: "incompatible existing records do not receive defaults",
    fn: testIncompatibleExistingRecordDoesNotReceiveDefaults,
  },
  {
    name: "existing disabled record does not receive defaults",
    fn: testExistingDisabledRecordDoesNotReceiveDefaults,
  },
  {
    name: "empty, quarantined, and invalid records do not receive defaults",
    fn: testEmptyQuarantinedAndInvalidRecordsDoNotGetDefaults,
  },
  {
    name: "initial-default atomic write fault boundaries",
    fn: testInitialDefaultWriteFaultBoundaries,
  },
  {
    name: "app-menu readiness and generation fencing",
    fn: testAppMenuReadinessAndGenerationFencing,
  },
  {
    name: "reentrant adapter replacement fences old acknowledgement",
    fn: testReentrantAdapterReplacementFencesOldAcknowledgement,
  },
  {
    name: "missing adapter timeout and HMR rebind",
    fn: testMissingAdapterAndHmrRebindAreBounded,
  },
  {
    name: "in-flight app-menu publication failure settles once",
    fn: testInFlightPublicationFailureSettlesOnce,
  },
  {
    name: "late app-menu failure preserves terminal replay",
    fn: testLateMenuFailurePreservesTerminalReplay,
  },
  {
    name: "terminal durable-write observer failure preserves the outcome",
    fn: testTerminalWriteObserverFailurePreservesOutcome,
  },
  {
    name: "late readiness timeout blocks the published host without resetting",
    fn: testLateReadinessTimeoutBlocksPublishedHost,
  },
  {
    name: "true window closure cancels the published readiness wait",
    fn: testWindowClosureCancelsPublishedReadinessWait,
  },
  {
    name: "generic host does not close an app-menu connection",
    fn: testGenericHostDoesNotCloseAppMenuConnection,
  },
  {
    name: "app-menu cleanup errors block generation replacement",
    fn: testMenuCleanupErrorsBlockReplacement,
  },
  {
    name: "replacement and disable use distinct app-menu removal reasons",
    fn: testReplacementAndDisableRemovalReasons,
  },
  {
    name: "disconnect and close cleanup failures are sticky",
    fn: testDisconnectAndCloseCleanupFailuresAreSticky,
  },
];

await runTests("FeatureHostStage2.test.mts", tests);
