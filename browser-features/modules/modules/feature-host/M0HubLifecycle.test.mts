// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  M0FeatureHostTestOptions,
  M0TestHostHandle,
} from "./FeatureHost.sys.mts";
import type {
  HubClientConnection,
  HubClientDiscoveryAdapter,
  HubLiveDocument,
} from "./HubRegistryTypes.ts";

type FeatureHostModule = typeof import("./FeatureHost.sys.mts");
const { createM0FeatureHostForTests } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as FeatureHostModule;
type RegistryModule = typeof import("./HubRegistry.sys.mts");
const { HubRegistry } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/HubRegistry.sys.mjs",
) as RegistryModule;

function assert(condition: boolean, message = "assertion failed"): void {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "values should match",
): void {
  harnessAssertEquals(actual, expected, message);
}

class LifecycleConnection implements HubClientConnection {
  readonly clientId: string;
  readonly connectionId: string;
  readonly windowId: string;
  readonly #closedListeners = new Set<() => void>();
  closed = false;
  #lastSnapshot: string | null = null;
  constructor(id: string, windowId: string) {
    this.clientId = `hub-client-${id}`;
    this.connectionId = `hub-connection-${id}`;
    this.windowId = windowId;
  }
  isClosed(): boolean {
    return this.closed;
  }
  sendSnapshot(json: string): void {
    this.#lastSnapshot = json;
    if (!this.discovery?.acknowledge) return;
    this.acknowledgeLatest();
  }
  acknowledgeLatest(): void {
    if (!this.#lastSnapshot) return;
    const message = JSON.parse(this.#lastSnapshot) as {
      snapshot: { epoch: string; hubRevision: number };
    };
    const acknowledgement = JSON.stringify({
      type: "snapshot-ack",
      epoch: message.snapshot.epoch,
      hubRevision: message.snapshot.hubRevision,
    });
    void Promise.resolve().then(() =>
      this.discovery?.registry?.acknowledge(this, acknowledgement)
    );
  }
  onClosed(callback: () => void): () => void {
    this.#closedListeners.add(callback);
    if (this.closed) callback();
    return () => this.#closedListeners.delete(callback);
  }
  close(): void {
    this.closed = true;
    for (const callback of [...this.#closedListeners]) callback();
  }
  discovery: LifecycleDiscovery | null = null;
}

class LifecycleDocument implements HubLiveDocument {
  readonly documentId: string;
  readonly #closedListeners = new Set<() => void>();
  #live = true;
  constructor(
    readonly windowId: string,
    readonly connection: LifecycleConnection,
  ) {
    this.documentId = `${connection.clientId}-document`;
  }
  isLive(): boolean {
    return this.#live;
  }
  ensureParentClient(): HubClientConnection | null {
    return this.connection;
  }
  onParentClientAvailable(_callback: () => void): () => void {
    return () => {};
  }
  onClosed(callback: () => void): () => void {
    this.#closedListeners.add(callback);
    if (!this.#live) callback();
    return () => this.#closedListeners.delete(callback);
  }
  close(): void {
    this.#live = false;
    for (const callback of [...this.#closedListeners]) callback();
  }
}

class LifecycleDiscovery implements HubClientDiscoveryAdapter {
  registry: InstanceType<RegistryModule["HubRegistry"]> | null = null;
  acknowledge = true;
  failNextDiscovery = false;
  readonly #connections = new Map<string, LifecycleConnection>();

  discoverLiveHubDocuments(
    windowIds: readonly string[],
  ): readonly HubLiveDocument[] {
    if (this.failNextDiscovery) {
      this.failNextDiscovery = false;
      throw new Error("injected one-shot HUB discovery failure");
    }
    return windowIds.map((windowId) => {
      let connection = this.#connections.get(windowId);
      if (!connection) {
        connection = new LifecycleConnection(windowId, windowId);
        connection.discovery = this;
        this.#connections.set(windowId, connection);
      }
      return new LifecycleDocument(windowId, connection);
    });
  }

  closeAllClients(): void {
    for (const connection of this.#connections.values()) connection.close();
  }

  acknowledgeWindow(windowId: string): void {
    this.#connections.get(windowId)?.acknowledgeLatest();
  }
}

async function waitForCondition(
  condition: () => boolean,
  label: string,
  timeoutMs = 5_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!condition()) {
    if (Date.now() >= deadline) throw new Error(`${label} timed out`);
    await new Promise<void>((resolve) => setTimeout(resolve, 10));
  }
}

interface JournalRecord {
  desiredEnabled: boolean;
  selectedVersion: string | null;
  blockedReason: string | null;
  lastOutcome: { status: string } | null;
  attempt: { attemptedDigests: string[]; rejectedDigests: string[] } | null;
  stateRevision: number;
}

async function journal(session: M0TestHostHandle): Promise<JournalRecord> {
  return await IOUtils.readJSON(
    PathUtils.join(session.root, "state.json"),
  ) as JournalRecord;
}

async function openHost(
  discovery: LifecycleDiscovery,
  testOptions: Omit<M0FeatureHostTestOptions, "hubRegistryForTests"> = {},
  root?: string,
  reopenTestRoot = false,
): Promise<
  {
    session: M0TestHostHandle;
    registry: InstanceType<RegistryModule["HubRegistry"]>;
  }
> {
  const registry = new HubRegistry({ discovery });
  discovery.registry = registry;
  const session = await createM0FeatureHostForTests(
    true,
    root,
    reopenTestRoot,
    undefined,
    {
      ...testOptions,
      hubRegistryForTests: registry,
    },
  );
  return { session, registry };
}

async function clean(session: M0TestHostHandle | null): Promise<void> {
  if (session) {
    await session.shutdownForRestartForTests();
    await session.cleanup();
  }
}

async function testCandidateManifestStagesAndPublishesAfterCommit(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  const visibilityAtCommit: number[] = [];
  let registry: InstanceType<RegistryModule["HubRegistry"]> | null = null;
  try {
    const opened = await openHost(discovery, {
      afterDurableWriteForTests: (snapshot) => {
        const outcome = snapshot.lastOutcome as { status?: string } | null;
        if (outcome?.status === "committed_pending_publish") {
          visibilityAtCommit.push(
            registry?.getSnapshotForTests().entries.length ?? -1,
          );
        }
        return Promise.resolve();
      },
    });
    session = opened.session;
    registry = opened.registry;
    const openedRegistry = opened.registry;
    const host = session.host;
    assertEquals(
      (await host.install("hub-install-v1", 0, "v1")).status,
      "succeeded",
    );
    assertEquals(
      (await host.enable("hub-enable-v1", host.diagnostics.stateRevision))
        .status,
      "succeeded",
    );
    const update = await host.update(
      "hub-upgrade-to-hub-v1",
      host.diagnostics.stateRevision,
      "hub-v1",
    );
    assertEquals(
      update.status,
      "succeeded",
      "candidate hub-pages manifest is checked, not selected v1's old manifest",
    );
    assertEquals(
      visibilityAtCommit.at(-1),
      0,
      "staged HUB entry is invisible at durable commit marker",
    );
    assertEquals(
      openedRegistry.getSnapshotForTests().entries[0]?.ownerGeneration,
      2,
    );
    assertEquals(
      openedRegistry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v1",
    );
    const second = await host.update(
      "hub-update-v2",
      host.diagnostics.stateRevision,
      "hub-v2",
    );
    assertEquals(second.status, "succeeded");
    assertEquals(
      openedRegistry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v2",
    );
    assert(
      openedRegistry.getSnapshotForTests().entries[0]?.ownerGeneration === 3,
    );
    assertEquals(
      (await host.rollback(
        "hub-positive-rollback",
        host.diagnostics.stateRevision,
      ))
        .status,
      "succeeded",
    );
    assertEquals(
      openedRegistry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v1",
    );
    assertEquals(
      (await host.disable("hub-disable", host.diagnostics.stateRevision))
        .status,
      "succeeded",
    );
    assertEquals(openedRegistry.getSnapshotForTests().entries.length, 0);
    assertEquals(openedRegistry.diagnosticsForTests.pendingAcknowledgements, 0);
    assertEquals(
      (await host.enable(
        "hub-positive-reenable",
        host.diagnostics.stateRevision,
      ))
        .status,
      "succeeded",
    );
    assertEquals(
      openedRegistry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v1",
    );
  } finally {
    await clean(session);
  }
}

async function testStagedThrowDoesNotPublish(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery);
    session = opened.session;
    const host = session.host;
    assertEquals(
      (await host.install("hub-install-throw", 0, "hub-staged-failure")).status,
      "succeeded",
    );
    const failed = await host.enable(
      "hub-enable-throw",
      host.diagnostics.stateRevision,
    );
    assertEquals(failed.status, "failed_restored");
    assertEquals(opened.registry.getSnapshotForTests().entries.length, 0);
    assertEquals(opened.registry.diagnosticsForTests.stagedEntries, 0);
  } finally {
    await clean(session);
  }
}

async function testStagedCandidateFailureRestoresHealthyOldHub(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery);
    session = opened.session;
    await session.host.install("hub-restore-install", 0, "hub-v1");
    await session.host.enable(
      "hub-restore-enable",
      session.host.diagnostics.stateRevision,
    );
    const result = await session.host.update(
      "hub-staged-candidate-failure-restores-old",
      session.host.diagnostics.stateRevision,
      "hub-staged-failure",
    );
    assertEquals(result.status, "failed_restored");
    assertEquals(session.host.diagnostics.selectedVersion, "hub-v1");
    assertEquals(session.host.diagnostics.blockedReason, null);
    assertEquals(
      opened.registry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v1",
    );
    assertEquals(opened.registry.diagnosticsForTests.stagedEntries, 0);
    assertEquals(opened.registry.diagnosticsForTests.leases, 1);
  } finally {
    await clean(session);
  }
}

async function testRecoveryPrecommitFailureKeepsHealthyFallback(): Promise<
  void
> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const initial = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 5_000,
    });
    first = initial.session;
    await first.host.install("hub-recovery-install", 0, "hub-v1");
    await first.host.enable(
      "hub-recovery-enable",
      first.host.diagnostics.stateRevision,
    );
    discovery.acknowledge = false;
    const update = first.host.update(
      "hub-recovery-committed-v2",
      first.host.diagnostics.stateRevision,
      "hub-v2",
    );
    void update.catch(() => {});
    await waitForCondition(
      () =>
        first?.committedPendingPublishRecordForTests() !== null &&
        initial.registry.diagnosticsForTests.pendingAcknowledgements > 0,
      "durable committed update pending publication",
    );
    const committed = first.committedPendingPublishRecordForTests();
    assert(committed !== null, "captured durable committed-pending record");
    const root = first.root;
    await first.shutdownForRestartForTests();
    await IOUtils.writeJSON(PathUtils.join(root, "state.json"), committed, {
      flush: true,
    });
    discovery.acknowledge = true;
    discovery.failNextDiscovery = true;
    const recovered = await openHost(
      discovery,
      { hubPublicationTimeoutMsForTests: 5_000 },
      root,
      true,
    );
    reopened = recovered.session;
    assertEquals(reopened.host.diagnostics.selectedVersion, "hub-v2");
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.status,
      "failed_quarantined",
    );
    const persisted = await journal(reopened);
    assertEquals(persisted.blockedReason, "recovery-attempt-exhausted");
    assertEquals(persisted.attempt?.attemptedDigests.length, 2);
    assertEquals(persisted.attempt?.rejectedDigests.length, 0);
    assertEquals(reopened.host.evaluationCountForTests, 0);
    assertEquals(recovered.registry.getSnapshotForTests().entries.length, 0);
    assertEquals(
      recovered.registry.diagnosticsForTests.pendingAcknowledgements,
      0,
    );
  } finally {
    await clean(reopened ?? first);
    discovery.closeAllClients();
  }
}

async function testTerminalStartupRecoveryFallsBackToHealthyRollback(): Promise<
  void
> {
  let first: M0TestHostHandle | null = null;
  let reopened: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const initial = await openHost(discovery);
    first = initial.session;
    await first.host.install("hub-startup-fallback-install", 0, "hub-v1");
    await first.host.enable(
      "hub-startup-fallback-enable",
      first.host.diagnostics.stateRevision,
    );
    assertEquals(
      (await first.host.update(
        "hub-startup-fallback-update",
        first.host.diagnostics.stateRevision,
        "hub-v2",
      )).status,
      "succeeded",
    );
    const root = first.root;
    await first.shutdownForRestartForTests();
    discovery.failNextDiscovery = true;
    const recovered = await openHost(discovery, {}, root, true);
    reopened = recovered.session;
    assertEquals(reopened.host.diagnostics.selectedVersion, "hub-v1");
    assertEquals(reopened.host.diagnostics.lastOutcome?.status, "succeeded");
    assertEquals(
      reopened.host.diagnostics.lastOutcome?.request.op,
      "startup_recovery",
    );
    assertEquals(reopened.host.evaluationCountForTests, 1);
    const persisted = await journal(reopened);
    assertEquals(persisted.attempt, null);
    assertEquals(
      recovered.registry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v1",
    );
  } finally {
    await clean(reopened ?? first);
    discovery.closeAllClients();
  }
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

async function testConcurrentAdmissionWaitsAbortOnShutdown(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  const windows: Window[] = [];
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 10_000,
    });
    session = opened.session;
    await session.host.install("hub-admission-install", 0, "hub-v1");
    await session.host.enable(
      "hub-admission-enable",
      session.host.diagnostics.stateRevision,
    );
    const clientBaseline = opened.registry.diagnosticsForTests.clients;
    const initialWindowIds = new Set(session.host.windowIdsForTests);
    discovery.acknowledge = false;
    windows.push(openTestBrowserWindow(), openTestBrowserWindow());
    await waitForCondition(
      () =>
        windows.every((item) =>
          (item as Window & {
            gBrowserInit?: { delayedStartupFinished?: boolean };
          }).gBrowserInit?.delayedStartupFinished === true
        ),
      "two new browser windows finish delayed startup",
    );
    await waitForCondition(
      () =>
        session?.host.windowLifecycleDiagnosticsForTests.admissionTasks === 2 &&
        opened.registry.diagnosticsForTests.pendingAcknowledgements === 2 &&
        opened.registry.diagnosticsForTests.clients === clientBaseline + 2 &&
        windows.every((item) => {
          const id = session?.host.windowIdForTests(item);
          return id !== null && id !== undefined && !initialWindowIds.has(id);
        }),
      "two independent HUB admission waits",
    );
    let timeout = 0;
    const report = await Promise.race([
      session.shutdown(),
      new Promise<never>((_, reject) => {
        timeout = setTimeout(
          () => reject(new Error("shutdown waited for admission deadlines")),
          2_000,
        );
      }),
    ]).finally(() => clearTimeout(timeout));
    assertEquals(report.timedOut, false);
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.admissionTasks,
      0,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.pendingAcknowledgements,
      0,
    );
    assertEquals(opened.registry.diagnosticsForTests.pendingTimers, 0);
    assertEquals(opened.registry.diagnosticsForTests.claims, 0);
    assertEquals(opened.registry.diagnosticsForTests.leases, 0);
    assertEquals(
      opened.registry.diagnosticsForTests.clients,
      clientBaseline + 2,
      "live clients remain until actual connection close",
    );
    for (const item of windows) if (!item.closed) item.close();
    discovery.closeAllClients();
    assertEquals(opened.registry.diagnosticsForTests.clients, 0);
  } finally {
    for (const item of windows) if (!item.closed) item.close();
    await clean(session);
    discovery.closeAllClients();
  }
}

async function testFailedUpdatePreparationPreservesPendingWindowReadiness(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  let addedWindow: Window | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 5_000,
    });
    session = opened.session;
    await session.host.install("hub-update-admission-install", 0, "hub-v1");
    await session.host.enable(
      "hub-update-admission-enable",
      session.host.diagnostics.stateRevision,
    );
    const initialWindowIds = new Set(session.host.windowIdsForTests);
    const initialTrackedWindows =
      session.host.windowLifecycleDiagnosticsForTests.tracked;
    const registryBaseline = opened.registry.diagnosticsForTests;
    const initialClients = registryBaseline.clients;
    discovery.acknowledge = false;
    const admissionWindow = openTestBrowserWindow();
    addedWindow = admissionWindow;
    const activeSession = session;
    await waitForCondition(
      () =>
        (admissionWindow as Window & {
            gBrowserInit?: { delayedStartupFinished?: boolean };
          }).gBrowserInit?.delayedStartupFinished === true &&
        activeSession.host.windowIdForTests(admissionWindow) !== null,
      "new update-admission window startup",
    );
    const windowId = activeSession.host.windowIdForTests(admissionWindow);
    if (windowId === null || initialWindowIds.has(windowId)) {
      throw new Error("new update-admission window was not tracked separately");
    }
    try {
      await waitForCondition(
        () =>
          session?.host.windowLifecycleDiagnosticsForTests.admissionTasks ===
            1 &&
          session.host.windowLifecycleDiagnosticsForTests.tracked ===
            initialTrackedWindows + 1 &&
          session.host.windowIdsForTests.length === initialWindowIds.size + 1 &&
          opened.registry.diagnosticsForTests.pendingAcknowledgements === 1 &&
          opened.registry.diagnosticsForTests.clients === initialClients + 1,
        "new update-admission readiness wait",
      );
    } catch (error) {
      throw new Error(
        `${String(error)}; admission=${
          JSON.stringify(session.host.windowLifecycleDiagnosticsForTests)
        }; registry=${JSON.stringify(opened.registry.diagnosticsForTests)}`,
      );
    }
    session.failNextWriteForTests("before_replace");
    const failed = await session.host.update(
      "hub-update-before-replace",
      session.host.diagnostics.stateRevision,
      "hub-v2",
    );
    assertEquals(failed.status, "storage_failed");
    const record = await journal(session);
    assertEquals(record.desiredEnabled, true);
    assertEquals(record.selectedVersion, "hub-v1");
    assertEquals(record.blockedReason, null);
    assert(session.host.runningScopeDiagnosticsForTests.resources > 0);
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.tracked,
      initialTrackedWindows + 1,
    );
    assertEquals(
      session.host.windowIdsForTests.length,
      initialWindowIds.size + 1,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.pendingAcknowledgements,
      1,
    );
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.admissionTasks,
      1,
    );
    discovery.acknowledgeWindow(windowId);
    await waitForCondition(
      () =>
        session?.host.windowLifecycleDiagnosticsForTests.admissionTasks === 0 &&
        opened.registry.diagnosticsForTests.pendingAcknowledgements === 0,
      "update admission resolves after a valid acknowledgement",
    );
    assert(session.host.windowIdsForTests.includes(windowId));
    assertEquals(session.host.diagnostics.blockedReason, null);
    assertEquals(
      opened.registry.diagnosticsForTests.claims,
      registryBaseline.claims,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.leases,
      registryBaseline.leases,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.clients,
      initialClients + 1,
    );
    assertEquals(opened.registry.diagnosticsForTests.pendingTimers, 0);
  } finally {
    try {
      if (addedWindow && !addedWindow.closed) addedWindow.close();
    } finally {
      try {
        await clean(session);
      } finally {
        discovery.closeAllClients();
      }
    }
  }
}

async function testFailedDisablePreparationRetainsTimedOutReadiness(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  let addedWindow: Window | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 250,
    });
    session = opened.session;
    await session.host.install("hub-disable-admission-install", 0, "hub-v1");
    await session.host.enable(
      "hub-disable-admission-enable",
      session.host.diagnostics.stateRevision,
    );
    const initialWindowIds = new Set(session.host.windowIdsForTests);
    const initialTrackedWindows =
      session.host.windowLifecycleDiagnosticsForTests.tracked;
    const registryBaseline = opened.registry.diagnosticsForTests;
    const initialClients = registryBaseline.clients;
    discovery.acknowledge = false;
    const admissionWindow = openTestBrowserWindow();
    addedWindow = admissionWindow;
    const activeSession = session;
    await waitForCondition(
      () =>
        (admissionWindow as Window & {
            gBrowserInit?: { delayedStartupFinished?: boolean };
          }).gBrowserInit?.delayedStartupFinished === true &&
        activeSession.host.windowIdForTests(admissionWindow) !== null,
      "new disable-admission window startup",
    );
    const windowId = activeSession.host.windowIdForTests(admissionWindow);
    if (windowId === null || initialWindowIds.has(windowId)) {
      throw new Error(
        "new disable-admission window was not tracked separately",
      );
    }
    try {
      await waitForCondition(
        () =>
          session?.host.windowLifecycleDiagnosticsForTests.admissionTasks ===
            1 &&
          session.host.windowLifecycleDiagnosticsForTests.tracked ===
            initialTrackedWindows + 1 &&
          session.host.windowIdsForTests.length === initialWindowIds.size + 1 &&
          opened.registry.diagnosticsForTests.pendingAcknowledgements === 1 &&
          opened.registry.diagnosticsForTests.clients === initialClients + 1,
        "new disable-admission readiness wait",
      );
    } catch (error) {
      throw new Error(
        `${String(error)}; admission=${
          JSON.stringify(session.host.windowLifecycleDiagnosticsForTests)
        }; registry=${JSON.stringify(opened.registry.diagnosticsForTests)}`,
      );
    }
    session.failNextWriteForTests("before_replace");
    const failed = await session.host.disable(
      "hub-disable-before-replace",
      session.host.diagnostics.stateRevision,
    );
    assertEquals(failed.status, "storage_failed");
    const record = await journal(session);
    assertEquals(record.desiredEnabled, true);
    assertEquals(record.selectedVersion, "hub-v1");
    assertEquals(record.blockedReason, null);
    assert(session.host.runningScopeDiagnosticsForTests.resources > 0);
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.tracked,
      initialTrackedWindows + 1,
    );
    assertEquals(
      session.host.windowIdsForTests.length,
      initialWindowIds.size + 1,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.pendingAcknowledgements,
      1,
    );
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.admissionTasks,
      1,
    );
    await waitForCondition(
      () => session?.host.diagnostics.blockedReason !== null,
      "admission timeout quarantines the resumed active generation",
    );
    const quarantined = await journal(session);
    assertEquals(quarantined.desiredEnabled, true);
    assertEquals(quarantined.selectedVersion, "hub-v1");
    assertEquals(
      quarantined.blockedReason,
      "postcommit-window-attach-failure",
    );
    assertEquals(session.host.runningScopeDiagnosticsForTests.resources, 0);
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.admissionTasks,
      0,
    );
    assertEquals(
      session.host.windowLifecycleDiagnosticsForTests.tracked,
      initialTrackedWindows + 1,
    );
    assertEquals(session.host.windowIdsForTests.length, 0);
    assertEquals(
      opened.registry.diagnosticsForTests.claims,
      registryBaseline.claims,
    );
    assertEquals(opened.registry.diagnosticsForTests.leases, 0);
    assertEquals(opened.registry.diagnosticsForTests.publishedEntries, 0);
    assertEquals(
      opened.registry.diagnosticsForTests.clients,
      initialClients + 1,
    );
    assertEquals(
      opened.registry.diagnosticsForTests.pendingAcknowledgements,
      0,
    );
    assertEquals(opened.registry.diagnosticsForTests.pendingTimers, 0);
  } finally {
    try {
      if (addedWindow && !addedWindow.closed) addedWindow.close();
    } finally {
      try {
        await clean(session);
      } finally {
        discovery.closeAllClients();
      }
    }
  }
}

async function testTenHubUpdatesReturnToCleanupBaseline(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let registry: InstanceType<RegistryModule["HubRegistry"]> | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery);
    session = opened.session;
    registry = opened.registry;
    await session.host.install("hub-cycle-install", 0, "hub-v1");
    await session.host.enable(
      "hub-cycle-enable",
      session.host.diagnostics.stateRevision,
    );
    const baseline = registry.diagnosticsForTests;
    for (let cycle = 0; cycle < 10; cycle += 1) {
      const version = cycle % 2 === 0 ? "hub-v2" : "hub-v1";
      const result = await session.host.update(
        `hub-cycle-${cycle}`,
        session.host.diagnostics.stateRevision,
        version,
      );
      assertEquals(result.status, "succeeded");
      const current = registry.diagnosticsForTests;
      assertEquals(current.claims, baseline.claims);
      assertEquals(current.leases, baseline.leases);
      assertEquals(current.stagedEntries, 0);
      assertEquals(current.pendingAcknowledgements, 0);
      assertEquals(current.pendingTimers, 0);
      assertEquals(current.clients, baseline.clients);
    }
  } finally {
    await clean(session);
    discovery.closeAllClients();
  }
  assert(registry !== null, "registry was created");
  assertEquals(registry.diagnosticsForTests.claims, 0);
  assertEquals(registry.diagnosticsForTests.leases, 0);
  assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
  assertEquals(registry.diagnosticsForTests.pendingTimers, 0);
  assertEquals(registry.diagnosticsForTests.clients, 0);
}

async function testPostCommitPublicationFailureQuarantinesCommittedSelection(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  discovery.acknowledge = false;
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 30,
    });
    session = opened.session;
    const host = session.host;
    await host.install("hub-install-timeout", 0, "hub-v1");
    const result = await host.enable(
      "hub-enable-timeout",
      host.diagnostics.stateRevision,
    );
    assertEquals(result.status, "failed_quarantined");
    const record = await journal(session);
    assertEquals(
      record.selectedVersion,
      "hub-v1",
      "committed candidate is not restored over",
    );
    assertEquals(record.desiredEnabled, true);
    assert(record.blockedReason !== null);
    assertEquals(opened.registry.getSnapshotForTests().entries.length, 0);
  } finally {
    await clean(session);
  }
}

async function testDisableRemovalFailureKeepsFalseIntent(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 30,
    });
    session = opened.session;
    const host = session.host;
    await host.install("hub-install-disable-timeout", 0, "hub-v1");
    await host.enable(
      "hub-enable-disable-timeout",
      host.diagnostics.stateRevision,
    );
    discovery.acknowledge = false;
    const result = await host.disable(
      "hub-disable-timeout",
      host.diagnostics.stateRevision,
    );
    assertEquals(result.status, "failed_quarantined");
    const record = await journal(session);
    assertEquals(record.desiredEnabled, false);
    assertEquals(record.selectedVersion, "hub-v1");
    assertEquals(opened.registry.getSnapshotForTests().entries.length, 0);
  } finally {
    await clean(session);
  }
}

async function testTerminalStorageFailureAfterHubAckDoesNotRestoreOldGeneration(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  try {
    const opened = await openHost(discovery, {
      afterDurableWriteForTests: (snapshot) => {
        const outcome = snapshot.lastOutcome as { status?: string } | null;
        if (outcome?.status === "committed_pending_publish") {
          session?.failNextWriteForTests("before_replace");
        }
        return Promise.resolve();
      },
    });
    session = opened.session;
    const host = session.host;
    await host.install("hub-install-storage", 0, "hub-v1");
    const result = await host.enable(
      "hub-enable-storage",
      host.diagnostics.stateRevision,
    );
    assertEquals(result.status, "storage_failed");
    const record = await journal(session);
    assertEquals(record.selectedVersion, "hub-v1");
    assertEquals(record.desiredEnabled, true);
    assert(record.blockedReason !== null);
    assertEquals(opened.registry.getSnapshotForTests().entries.length, 0);
  } finally {
    await clean(session);
  }
}

async function testShutdownAbortsManagedHubPublicationWait(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  const discovery = new LifecycleDiscovery();
  discovery.acknowledge = false;
  try {
    const opened = await openHost(discovery, {
      hubPublicationTimeoutMsForTests: 1_000,
    });
    session = opened.session;
    const host = session.host;
    await host.install("hub-install-shutdown", 0, "hub-v1");
    const enabling = host.enable(
      "hub-enable-shutdown",
      host.diagnostics.stateRevision,
    );
    const deadline = Date.now() + 1_000;
    while (opened.registry.diagnosticsForTests.pendingAcknowledgements === 0) {
      if (Date.now() >= deadline) {
        throw new Error("HUB publication wait did not start");
      }
      await new Promise<void>((resolve) => setTimeout(resolve, 5));
    }
    const shutdown = host.shutdown();
    const result = await enabling;
    const report = await shutdown;
    assertEquals(result.status, "failed_quarantined");
    assertEquals(report.errors.length, 0);
    assertEquals(report.timedOut, false);
    assertEquals(
      opened.registry.diagnosticsForTests.pendingAcknowledgements,
      0,
    );
    assertEquals(opened.registry.getSnapshotForTests().entries.length, 0);
  } finally {
    await clean(session);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name:
        "FeatureHost stages HUB pages and publishes acknowledged candidate snapshots",
      fn: testCandidateManifestStagesAndPublishesAfterCommit,
    },
    {
      name:
        "a staged HUB registration is discarded when candidate activation throws",
      fn: testStagedThrowDoesNotPublish,
    },
    {
      name:
        "staged HUB candidate failure restores a healthy old HUB generation",
      fn: testStagedCandidateFailureRestoresHealthyOldHub,
    },
    {
      name:
        "recovery pre-commit failure preserves the healthy distinct fallback",
      fn: testRecoveryPrecommitFailureKeepsHealthyFallback,
    },
    {
      name:
        "terminal startup recovery uses the healthy rollback after a pre-start fault",
      fn: testTerminalStartupRecoveryFallsBackToHealthyRollback,
    },
    {
      name: "two HUB admissions are revoked before shutdown drains",
      fn: testConcurrentAdmissionWaitsAbortOnShutdown,
    },
    {
      name: "failed update preparation preserves pending admission readiness",
      fn: testFailedUpdatePreparationPreservesPendingWindowReadiness,
    },
    {
      name:
        "failed disable preparation retains admission readiness through timeout",
      fn: testFailedDisablePreparationRetainsTimedOutReadiness,
    },
    {
      name: "ten HUB updates return registry resources to the cleanup baseline",
      fn: testTenHubUpdatesReturnToCleanupBaseline,
    },
    {
      name:
        "post-commit HUB acknowledgement failure quarantines the committed selection",
      fn: testPostCommitPublicationFailureQuarantinesCommittedSelection,
    },
    {
      name: "failed HUB removal acknowledgement retains disable false intent",
      fn: testDisableRemovalFailureKeepsFalseIntent,
    },
    {
      name:
        "terminal storage failure after HUB acknowledgement does not restore an old generation",
      fn: testTerminalStorageFailureAfterHubAckDoesNotRestoreOldGeneration,
    },
    {
      name: "shutdown aborts a HUB publication wait without a queue cycle",
      fn: testShutdownAbortsManagedHubPublicationWait,
    },
  ];
  await runTests("M0HubLifecycle.test.mts", tests);
}
