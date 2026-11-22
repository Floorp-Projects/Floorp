// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import { launchDevPorts } from "../../common/dev-port-offset.ts";
import type {
  M0FeatureHostTestOptions,
  M0TestHostHandle,
} from "./FeatureHost.sys.mts";
import type {
  HubClientConnection,
  HubClientDiscoveryAdapter,
  HubLiveDocument,
} from "./HubRegistryTypes.ts";
import type {
  HubDomProbeRead,
  HubDomProbeTestGBrowser as TestGBrowser,
  HubDomProbeTestTab as TestTab,
  HubDomProbeTestUtilsModule,
  HubDomProbeTestWindowGlobal,
} from "./test/HubDomProbeTypes.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

type FeatureHostModule = typeof import("./FeatureHost.sys.mts");
const { createM0FeatureHostForTests } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as FeatureHostModule;
type RegistryModule = typeof import("./HubRegistry.sys.mts");
const { HubRegistry } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/HubRegistry.sys.mjs",
) as RegistryModule;
type HubPageDiscoveryModule = typeof import("./HubPageDiscovery.sys.mts");
const { HubPageDiscoveryService, initializeHubPageDiscovery } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/HubPageDiscovery.sys.mjs",
  ) as HubPageDiscoveryModule;
const {
  HUB_DOM_PROBE_ACTOR,
  readHubDomProbe,
  registerHubDomProbe,
  sendHubDomProbeCommand,
  waitForHubDomProbe,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/test/HubDomProbeTestUtils.sys.mjs",
) as HubDomProbeTestUtilsModule;
type WindowIdentityModule = typeof import("./WindowIdentity.sys.mts");
const { getFeatureWindowIdentity } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/WindowIdentity.sys.mjs",
) as WindowIdentityModule;
const TEST_SETTINGS_ORIGIN = launchDevPorts().httpOrigin("settings");
const TEST_SETTINGS_BASE = Services.prefs.getBoolPref(
    "nora.dev.allow_http_loader",
    false,
  )
  ? TEST_SETTINGS_ORIGIN
  : "chrome://noraneko-settings/content/index.html";
const TEST_SETTINGS_URL = `${TEST_SETTINGS_BASE}#/overview/home`;
const TEST_ABOUT_HUB_URL = "about:hub#/overview/home";
const HUB_FIXTURE_ROUTE = "/extensions/experimental-local-sample/sample";

function isTestSettingsDocument(uri: nsIURI | null): boolean {
  return uri?.spec.startsWith(TEST_SETTINGS_BASE) === true ||
    uri?.spec.split(/[?#]/)[0] === "about:hub";
}

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

function isHubVersionRendered(
  probe: HubDomProbeRead,
  version: "hub-v1" | "hub-v2",
): boolean {
  const englishTitle = `Hotfix sample (${version})`;
  const japaneseTitle = `ホットフィックスのサンプル（${version}）`;
  const englishBody = `The ${version} feature hotfix is active.`;
  const japaneseBody = `${version} の機能ホットフィックスが有効です。`;
  return probe.settingsRootReady && probe.hash === `#${HUB_FIXTURE_ROUTE}` &&
    probe.messageTitle !== null &&
    [englishTitle, japaneseTitle].includes(probe.messageTitle) &&
    probe.messageBody !== null &&
    [englishBody, japaneseBody].includes(probe.messageBody) &&
    probe.dynamicLinks.some((link) =>
      link.route === HUB_FIXTURE_ROUTE &&
      ["Hotfix sample", "ホットフィックスのサンプル"].includes(link.label)
    );
}

function isHubUnavailable(probe: HubDomProbeRead): boolean {
  return probe.settingsRootReady && probe.hash === `#${HUB_FIXTURE_ROUTE}` &&
    probe.messageTitle === null && probe.messageBody === null &&
    ["Feature page unavailable", "機能ページを利用できません"].includes(
      probe.unavailableTitle ?? "",
    ) &&
    [
      "This feature page is no longer available.",
      "この機能ページは利用できなくなりました。",
    ].includes(probe.unavailableBody ?? "") &&
    probe.dynamicLinks.length === 0 &&
    probe.builtInHomeLink?.route === "/overview/home";
}

function isHubHome(probe: HubDomProbeRead): boolean {
  return probe.settingsRootReady && probe.hash === "#/overview/home" &&
    probe.messageTitle === null && probe.unavailableTitle === null &&
    probe.builtInHomeLink?.route === "/overview/home" &&
    ["Home", "ホーム"].includes(probe.builtInHomeLink.label);
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
    try {
      await session.shutdownForRestartForTests();
    } finally {
      await session.cleanup();
    }
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

async function testPackagedHostAndDiscoveryShareWindowAndRegistryIdentity(): Promise<
  void
> {
  let session: M0TestHostHandle | null = null;
  try {
    session = await createM0FeatureHostForTests(true);
    const discoveryRegistry = HubPageDiscoveryService.sharedRegistryForTests();
    assertEquals(initializeHubPageDiscovery(), HubPageDiscoveryService);
    assertEquals(initializeHubPageDiscovery(), HubPageDiscoveryService);
    assert(HubPageDiscoveryService.usesConfiguredDiscoveryForTests());
    const sharedDiscovery = HubPageDiscoveryService.discoveryIdentityForTests();
    const repeatedDiscoveryImport = ChromeUtils.importESModule(
      "resource://noraneko/modules/feature-host/HubPageDiscovery.sys.mjs",
    ) as { HubPageDiscoveryService: typeof HubPageDiscoveryService };
    assert(
      repeatedDiscoveryImport.HubPageDiscoveryService
        .discoveryIdentityForTests() === sharedDiscovery,
      "packaged discovery imports share the singleton identity",
    );
    const host = session.host;
    await host.install("hub-install-packaged-singleton", 0, "hub-v1");
    const result = await host.enable(
      "hub-enable-packaged-singleton",
      host.diagnostics.stateRevision,
    );
    assertEquals(result.status, "succeeded");
    assert(
      discoveryRegistry.getSnapshotForTests().entries.length > 0,
      "the discovery module sees the host's published registry snapshot",
    );
    const window = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as Window | null;
    if (!window) {
      throw new Error("the packaged browser test has no main window");
    }
    const deadline = Date.now() + 1_000;
    let hostWindowId = host.windowIdForTests(window);
    while (hostWindowId === null && Date.now() < deadline) {
      await new Promise<void>((resolve) => setTimeout(resolve, 5));
      hostWindowId = host.windowIdForTests(window);
    }
    assert(hostWindowId !== null, "FeatureHost attached the main window");
    assertEquals(hostWindowId, getFeatureWindowIdentity(window));
  } finally {
    await clean(session);
  }
}

async function testClosingUnrelatedTabKeepsHubDocumentLive(): Promise<void> {
  const mainWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as (Window & { gBrowser?: TestGBrowser }) | null;
  if (!mainWindow?.gBrowser) {
    throw new Error("the packaged test browser is unavailable");
  }
  const gBrowser = mainWindow.gBrowser;
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  const clientBaseline = registry.diagnosticsForTests.clients;
  let hubTab: TestTab | null = null;
  let unrelatedTab: TestTab | null = null;
  try {
    hubTab = gBrowser.addTab(
      TEST_SETTINGS_URL,
      {
        inBackground: true,
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    unrelatedTab = gBrowser.addTab("about:blank", {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    await waitForCondition(
      () =>
        isTestSettingsDocument(
          hubTab?.linkedBrowser.browsingContext.currentWindowGlobal
            ?.documentURI ??
            null,
        ),
      "the live HUB tab loads",
    );
    const windowId = getFeatureWindowIdentity(mainWindow);
    const initialDocuments = registry.captureW0([windowId]);
    assertEquals(initialDocuments.length, 1);
    const initialDocumentId = initialDocuments[0].documentId;
    gBrowser.removeTab(unrelatedTab);
    unrelatedTab = null;
    const afterClose = registry.captureW0([windowId]);
    assertEquals(afterClose.length, 1);
    assertEquals(
      afterClose[0].documentId,
      initialDocumentId,
      "an unrelated TabClose does not close the live HUB document",
    );
  } finally {
    try {
      if (unrelatedTab?.isConnected) gBrowser.removeTab(unrelatedTab);
      if (hubTab?.isConnected) gBrowser.removeTab(hubTab);
    } finally {
      await waitForCondition(
        () => registry.diagnosticsForTests.clients === clientBaseline,
        "owned HUB client is removed after its own tab closes",
      );
    }
  }
}

async function testActorTeardownKeepsLiveW0PublicationPending(): Promise<void> {
  let tab: TestTab | null = null;
  let probeRegistered = false;
  let claim:
    | ReturnType<
      ReturnType<
        typeof HubPageDiscoveryService.sharedRegistryForTests
      >["claimHost"]
    >
    | null = null;
  let lease:
    | ReturnType<
      ReturnType<
        typeof HubPageDiscoveryService.sharedRegistryForTests
      >["beginGeneration"]
    >
    | null = null;
  let controller: AbortController | null = null;
  let publication: Promise<unknown> | null = null;
  let publicationState: "pending" | "fulfilled" | "rejected" = "pending";
  const mainWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as (Window & { gBrowser?: TestGBrowser }) | null;
  if (!mainWindow?.gBrowser) {
    throw new Error("the test browser is unavailable");
  }
  const gBrowser = mainWindow.gBrowser;
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  const clientBaseline = registry.diagnosticsForTests.clients;
  const windowId = getFeatureWindowIdentity(mainWindow);
  try {
    registerHubDomProbe();
    probeRegistered = true;
    tab = gBrowser.addTab(TEST_SETTINGS_URL, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    await waitForCondition(
      () =>
        isTestSettingsDocument(
          tab?.linkedBrowser.browsingContext.currentWindowGlobal?.documentURI ??
            null,
        ),
      "the parent-teardown settings document loads",
    );
    const initial = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.requestId !== null,
      "the live settings page completes its initial HUB handshake",
    );
    const manager = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!manager) throw new Error("the test WindowGlobal is unavailable");
    const actor = manager.getActor("NRFeatureHub");
    if (!actor) throw new Error("the NRFeatureHub parent actor is unavailable");
    const parentActor = actor as Parameters<
      typeof HubPageDiscoveryService.disconnectActor
    >[0];
    const parentManager = manager as Parameters<
      typeof HubPageDiscoveryService.connectActor
    >[1];
    const requestId = initial.protocol.latest.requestId;
    if (!requestId) throw new Error("the page request ID is unavailable");
    const documents = registry.captureW0([windowId]);
    assertEquals(documents.length, 1);
    assert(documents[0].isLive(), "W0 retains the actual settings document");
    const documentId = documents[0].documentId;

    const beforePublication = await readHubDomProbe(tab);
    const generationClaim = registry.claimHost(
      "parent-teardown",
      "parent-teardown-host",
    );
    claim = generationClaim;
    const generation = registry.beginGeneration({
      featureId: generationClaim.featureId,
      hostEpoch: generationClaim.hostEpoch,
      generation: 1,
    });
    lease = generation;
    generation.registerDescriptor(JSON.stringify({
      localId: "pending",
      route: "/extensions/parent-teardown/pending",
      titleKey: "parentTeardown.missingTitle",
      navigation: {
        labelKey: "parentTeardown.missingNavigation",
        order: 1,
      },
      view: {
        kind: "message",
        titleKey: "parentTeardown.missingTitle",
        bodyKey: "parentTeardown.missingBody",
      },
    }));
    registry.sealGeneration(generation);
    const w0 = registry.captureW0([windowId]);
    assertEquals(w0.length, 1);
    controller = new AbortController();
    publication = registry.publishGeneration(
      generation,
      w0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationState = "fulfilled",
      () => publicationState = "rejected",
    );
    await waitForCondition(
      () => registry.diagnosticsForTests.pendingAcknowledgements > 0,
      "the real page leaves the registry lease publication pending",
    );
    const pendingPage = await readHubDomProbe(tab);
    assert(
      pendingPage.protocol.snapshotCount >
        beforePublication.protocol.snapshotCount,
    );
    assertEquals(
      pendingPage.protocol.ackCount,
      beforePublication.protocol.ackCount,
      "the missing navigation translation withholds the publication ack",
    );

    HubPageDiscoveryService.disconnectActor(parentActor);
    const reconnected = HubPageDiscoveryService.connectActor(
      parentActor,
      parentManager,
      requestId,
    );
    assert(reconnected !== null, "a live non-retired actor can reconnect");
    HubPageDiscoveryService.closeActor(parentActor);
    assertEquals(
      HubPageDiscoveryService.connectActor(
        parentActor,
        parentManager,
        requestId,
      ),
      null,
      "a retired actor cannot reconnect",
    );
    const afterActorTeardown = registry.captureW0([windowId]);
    assertEquals(afterActorTeardown.length, 1);
    assertEquals(afterActorTeardown[0].documentId, documentId);
    assert(afterActorTeardown[0].isLive());
    assertEquals(
      afterActorTeardown[0].ensureParentClient(),
      null,
      "the retired actor is not reacquired by the live document",
    );
    await new Promise<void>((resolve) => setTimeout(resolve, 50));
    assertEquals(publicationState, "pending");
    assert(registry.diagnosticsForTests.pendingAcknowledgements > 0);

    gBrowser.removeTab(tab);
    tab = null;
    await waitForCondition(
      () => publicationState !== "pending",
      "closing the actual tab releases the W0 publication wait",
    );
    assertEquals(publicationState, "fulfilled");
    await publication;
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 0);
  } finally {
    try {
      try {
        if (tab?.isConnected) gBrowser.removeTab(tab);
      } finally {
        try {
          if (publicationState === "pending") {
            controller?.abort(new Error("parent-teardown test cleanup"));
          }
        } finally {
          try {
            await publication?.catch(() => {});
          } finally {
            try {
              lease?.dispose();
            } finally {
              try {
                claim?.release();
              } finally {
                await waitForCondition(
                  () => registry.diagnosticsForTests.clients === clientBaseline,
                  "parent-teardown test removes its HUB clients",
                );
              }
            }
          }
        }
      }
    } finally {
      if (probeRegistered) {
        ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
      }
    }
  }
}

async function testOwnedWindowCloseCompletesPendingPublicationAfterActorRetires(): Promise<
  void
> {
  let ownedWindow: Window | null = null;
  let ownedBrowser: TestGBrowser | null = null;
  let tab: TestTab | null = null;
  let probeRegistered = false;
  let claim:
    | ReturnType<
      ReturnType<
        typeof HubPageDiscoveryService.sharedRegistryForTests
      >["claimHost"]
    >
    | null = null;
  let lease:
    | ReturnType<
      ReturnType<
        typeof HubPageDiscoveryService.sharedRegistryForTests
      >["beginGeneration"]
    >
    | null = null;
  let controller: AbortController | null = null;
  let publication: Promise<unknown> | null = null;
  let publicationState: "pending" | "fulfilled" | "rejected" = "pending";
  const primaryWindowProxy = Services.wm.getMostRecentWindow(
    "navigator:browser",
  );
  if (!primaryWindowProxy) {
    throw new Error("the test runner browser window is unavailable");
  }
  const primaryWindow = primaryWindowProxy as unknown as Window;
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  initializeHubPageDiscovery();
  const registryBaseline = registry.diagnosticsForTests;
  try {
    registerHubDomProbe();
    probeRegistered = true;
    ownedWindow = openTestBrowserWindow(primaryWindowProxy);
    assert(
      ownedWindow !== primaryWindow,
      "the owned window is not the harness window",
    );
    const browserWindow = ownedWindow as Window & {
      gBrowser?: TestGBrowser;
      gBrowserInit?: { delayedStartupFinished?: boolean };
    };
    await waitForCondition(
      () =>
        !browserWindow.closed &&
        browserWindow.gBrowserInit?.delayedStartupFinished === true &&
        browserWindow.gBrowser !== undefined,
      "the owned second browser window finishes delayed startup",
      15_000,
    );
    const gBrowser = browserWindow.gBrowser;
    if (!gBrowser) {
      throw new Error("the owned browser window has no tab browser");
    }
    ownedBrowser = gBrowser;
    const windowId = getFeatureWindowIdentity(browserWindow);
    tab = gBrowser.addTab(TEST_SETTINGS_URL, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    await waitForCondition(
      () =>
        isTestSettingsDocument(
          tab?.linkedBrowser.browsingContext.currentWindowGlobal?.documentURI ??
            null,
        ),
      "the owned-window settings document loads",
    );
    await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.requestId !== null,
      "the owned-window settings page completes its initial HUB handshake",
    );
    const manager = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!manager) {
      throw new Error("the owned-window WindowGlobal is unavailable");
    }
    const actor = manager.getActor("NRFeatureHub");
    if (!actor) {
      throw new Error("the owned-window HUB parent actor is unavailable");
    }
    const parentActor = actor as Parameters<
      typeof HubPageDiscoveryService.disconnectActor
    >[0];
    const documents = registry.captureW0([windowId]);
    assertEquals(documents.length, 1);
    assert(documents[0].isLive(), "W0 retains the owned settings document");
    const beforePublication = await readHubDomProbe(tab);
    const generationClaim = registry.claimHost(
      "owned-window-close",
      "owned-window-close-host",
    );
    claim = generationClaim;
    const generation = registry.beginGeneration({
      featureId: generationClaim.featureId,
      hostEpoch: generationClaim.hostEpoch,
      generation: 1,
    });
    lease = generation;
    generation.registerDescriptor(JSON.stringify({
      localId: "pending",
      route: "/extensions/owned-window-close/pending",
      titleKey: "ownedWindowClose.missingTitle",
      navigation: {
        labelKey: "ownedWindowClose.missingNavigation",
        order: 1,
      },
      view: {
        kind: "message",
        titleKey: "ownedWindowClose.missingTitle",
        bodyKey: "ownedWindowClose.missingBody",
      },
    }));
    registry.sealGeneration(generation);
    const w0 = registry.captureW0([windowId]);
    assertEquals(w0.length, 1);
    controller = new AbortController();
    publication = registry.publishGeneration(
      generation,
      w0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationState = "fulfilled",
      () => publicationState = "rejected",
    );
    await waitForCondition(
      () =>
        registry.diagnosticsForTests.pendingAcknowledgements >
          registryBaseline.pendingAcknowledgements,
      "the owned-window HUB document leaves publication pending",
    );
    const pendingPage = await readHubDomProbe(tab);
    assert(
      pendingPage.protocol.snapshotCount >
        beforePublication.protocol.snapshotCount,
    );
    assertEquals(
      pendingPage.protocol.ackCount,
      beforePublication.protocol.ackCount,
      "the missing navigation translation withholds the publication ack",
    );

    HubPageDiscoveryService.disconnectActor(parentActor);
    HubPageDiscoveryService.closeActor(parentActor);
    const afterActorTeardown = registry.captureW0([windowId]);
    assertEquals(afterActorTeardown.length, 1);
    assert(afterActorTeardown[0].isLive());
    assertEquals(afterActorTeardown[0].ensureParentClient(), null);
    assertEquals(publicationState, "pending");
    assertEquals(
      registry.diagnosticsForTests.clients,
      registryBaseline.clients,
      "actor teardown removes its client without closing the live document",
    );

    browserWindow.close();
    await waitForCondition(
      () => browserWindow.closed,
      "the owned second browser window closes",
      15_000,
    );
    ownedWindow = null;
    ownedBrowser = null;
    tab = null;
    await waitForCondition(
      () => publicationState !== "pending",
      "closing the owned browser window releases its W0 publication wait",
    );
    assertEquals(publicationState, "fulfilled");
    await publication;
    await waitForCondition(
      () => {
        const diagnostics = registry.diagnosticsForTests;
        return diagnostics.clients === registryBaseline.clients &&
          diagnostics.pendingAcknowledgements ===
            registryBaseline.pendingAcknowledgements &&
          diagnostics.pendingTimers === registryBaseline.pendingTimers;
      },
      "owned-window close restores clients, waiters, and timers to baseline",
    );
  } finally {
    try {
      try {
        if (tab?.isConnected && ownedBrowser) ownedBrowser.removeTab(tab);
      } finally {
        try {
          if (
            ownedWindow && ownedWindow !== primaryWindow &&
            !ownedWindow.closed
          ) ownedWindow.close();
        } finally {
          try {
            if (publicationState === "pending") {
              controller?.abort(new Error("owned-window close test cleanup"));
            }
          } finally {
            try {
              await publication?.catch(() => {});
            } finally {
              try {
                lease?.dispose();
              } finally {
                try {
                  claim?.release();
                } finally {
                  await waitForCondition(
                    () => {
                      const diagnostics = registry.diagnosticsForTests;
                      return diagnostics.clients === registryBaseline.clients &&
                        diagnostics.pendingAcknowledgements ===
                          registryBaseline.pendingAcknowledgements &&
                        diagnostics.pendingTimers ===
                          registryBaseline.pendingTimers;
                    },
                    "owned-window test removes its HUB clients and publication waiters",
                  );
                }
              }
            }
          }
        }
      }
    } finally {
      if (probeRegistered) {
        ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
      }
    }
  }
}

async function testRealActorAndReactAcknowledgeHubPublication(): Promise<void> {
  let session: M0TestHostHandle | null = null;
  let tab: TestTab | null = null;
  let companionTab: TestTab | null = null;
  let probeRegistered = false;
  const mainWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as (Window & { gBrowser?: TestGBrowser }) | null;
  if (!mainWindow?.gBrowser) {
    throw new Error("the test browser is unavailable");
  }
  const gBrowser = mainWindow.gBrowser;
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  const clientBaseline = registry.diagnosticsForTests.clients;
  const windowId = getFeatureWindowIdentity(mainWindow);
  const initialWindowGlobals = new Map<TestTab, HubDomProbeTestWindowGlobal>();
  const ownedTabs = (): [TestTab, TestTab] => {
    if (!tab || !companionTab) {
      throw new Error("owned HUB tabs are unavailable");
    }
    return [tab, companionTab];
  };
  const assertWindowGlobalsUnchanged = (label: string): void => {
    for (const [index, ownedTab] of ownedTabs().entries()) {
      const expected = initialWindowGlobals.get(ownedTab);
      if (!expected) {
        throw new Error("the settings page WindowGlobal was not captured");
      }
      assertEquals(
        ownedTab.linkedBrowser.browsingContext.currentWindowGlobal,
        expected,
        `${label} keeps page ${index + 1} in the original WindowGlobal`,
      );
    }
  };
  const readBoth = async (): Promise<[HubDomProbeRead, HubDomProbeRead]> => {
    const [first, second] = ownedTabs();
    const results = await Promise.all([
      readHubDomProbe(first),
      readHubDomProbe(second),
    ]);
    return [results[0], results[1]];
  };
  const waitForBothIndexed = async (
    predicate: (probe: HubDomProbeRead, index: number) => boolean,
    label: string,
  ): Promise<[HubDomProbeRead, HubDomProbeRead]> => {
    const tabs = ownedTabs();
    const results = await Promise.allSettled(
      tabs.map((ownedTab, index) =>
        waitForHubDomProbe(
          ownedTab,
          (probe) => predicate(probe, index),
          `${label} in page ${index + 1}`,
        )
      ),
    );
    const failure = results.find((result) => result.status === "rejected");
    if (failure?.status === "rejected") {
      const pages = await Promise.allSettled(
        tabs.map((ownedTab) => readHubDomProbe(ownedTab)),
      );
      throw new Error(
        `${label}: ${String(failure.reason)}; HUB diagnostics: ${
          JSON.stringify({
            host: session?.host.diagnostics,
            registry: registry.diagnosticsForTests,
            registrySnapshot: registry.getSnapshotForTests(),
            pages,
          })
        }`,
      );
    }
    const [first, second] = results;
    if (first.status !== "fulfilled" || second.status !== "fulfilled") {
      throw new Error(`${label}: the two-page wait did not complete`);
    }
    return [first.value, second.value];
  };
  const waitForBoth = async (
    predicate: (probe: HubDomProbeRead) => boolean,
    label: string,
  ): Promise<[HubDomProbeRead, HubDomProbeRead]> =>
    await waitForBothIndexed((probe) => predicate(probe), label);
  const assertOperationSucceeded = async (
    result: { status: string },
    label: string,
  ): Promise<void> => {
    const pages = await Promise.allSettled(
      ownedTabs().map((ownedTab) => readHubDomProbe(ownedTab)),
    );
    assertEquals(
      result.status,
      "succeeded",
      `${label}: ${
        JSON.stringify({
          result,
          host: session?.host.diagnostics,
          registry: registry.diagnosticsForTests,
          pages,
        })
      }`,
    );
  };
  const assertPublishedVersion = (version: string): void => {
    assert(
      registry.getSnapshotForTests().entries.some(
        (entry) => entry.view.params?.version === version,
      ),
      `the real Actor/React snapshot publishes ${version}`,
    );
  };
  try {
    registerHubDomProbe();
    probeRegistered = true;
    tab = gBrowser.addTab(TEST_ABOUT_HUB_URL, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    gBrowser.selectedTab = tab;
    companionTab = gBrowser.addTab(TEST_SETTINGS_URL, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    gBrowser.selectedTab = tab;
    await waitForCondition(
      () =>
        isTestSettingsDocument(
          tab?.linkedBrowser.browsingContext.currentWindowGlobal?.documentURI ??
            null,
        ),
      "the actual top-level about:hub document loads",
    );
    const aboutHubManager = tab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!aboutHubManager) {
      throw new Error("the about:hub WindowGlobal is unavailable");
    }
    assertEquals(
      aboutHubManager.documentURI?.spec.split(/[?#]/)[0],
      "about:hub",
      "the lifecycle page is the actual about:hub document",
    );
    assertEquals(
      aboutHubManager.browsingContext.parent,
      null,
      "the about:hub handshake is top-level",
    );
    await waitForCondition(
      () =>
        isTestSettingsDocument(
          companionTab?.linkedBrowser.browsingContext.currentWindowGlobal
            ?.documentURI ?? null,
        ),
      "the companion settings HUB test document loads",
    );
    await Promise.all(
      ownedTabs().map((ownedTab, index) =>
        waitForHubDomProbe(
          ownedTab,
          (probe) =>
            probe.settingsRootReady &&
            probe.protocol.connectCount > 0 &&
            probe.protocol.latest.requestId !== null,
          `the ${
            index === 0 ? "about:hub" : "companion settings page"
          } connects`,
        )
      ),
    );
    for (const ownedTab of ownedTabs()) {
      const windowGlobal = ownedTab.linkedBrowser.browsingContext
        .currentWindowGlobal;
      if (!windowGlobal) {
        throw new Error("a settings page WindowGlobal is unavailable");
      }
      initialWindowGlobals.set(ownedTab, windowGlobal);
    }
    await waitForCondition(
      () => registry.captureW0([windowId]).length === 2,
      "parent discovery includes both live settings WindowGlobals",
    );
    await waitForCondition(
      () => registry.diagnosticsForTests.clients >= clientBaseline + 2,
      "the parent accepts both settings page connections",
    );
    await waitForBoth(
      (probe) =>
        probe.protocol.snapshotCount > 0 &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.connectionNonce !== null &&
        probe.protocol.latest.epoch !== null &&
        probe.protocol.latest.revision !== null,
      "both pages complete their initial snapshot handshake",
    );
    assertWindowGlobalsUnchanged("initial Actor handshakes");
    session = await createM0FeatureHostForTests(
      true,
      undefined,
      false,
      undefined,
      { hubPublicationTimeoutMsForTests: 10_000 },
    );
    const installed = await session.host.install(
      "hub-install-real-actor",
      0,
      "hub-v1",
    );
    await assertOperationSucceeded(installed, "install hub-v1");
    const enabled = await session.host.enable(
      "hub-enable-real-actor",
      session.host.diagnostics.stateRevision,
    );
    await assertOperationSucceeded(enabled, "enable hub-v1");
    assertPublishedVersion("hub-v1");
    await waitForBoth(
      (probe) =>
        probe.dynamicLinks.some((link) =>
          link.route === HUB_FIXTURE_ROUTE &&
          ["Hotfix sample", "ホットフィックスのサンプル"].includes(link.label)
        ),
      "dynamic navigation is present in both pages",
    );
    assertWindowGlobalsUnchanged("HUB enablement");
    const beforeRouteCommit = await readBoth();
    for (const ownedTab of ownedTabs()) {
      const response = await sendHubDomProbeCommand(
        ownedTab,
        "ClickRoute",
        HUB_FIXTURE_ROUTE,
      );
      assertEquals(
        response,
        JSON.stringify({ ok: true, route: HUB_FIXTURE_ROUTE }),
        "the fixed probe clicks the real dynamic route link",
      );
    }
    await waitForBothIndexed(
      (probe, index) =>
        isHubVersionRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > beforeRouteCommit[index].protocol.ackCount,
      "hub-v1 dynamic route commits and is acknowledged",
    );
    assertWindowGlobalsUnchanged("dynamic link navigation");

    const beforeUpdate = await readBoth();
    const updated = await session.host.update(
      "hub-update-real-actor-v2",
      session.host.diagnostics.stateRevision,
      "hub-v2",
    );
    await assertOperationSucceeded(updated, "update to hub-v2");
    assertPublishedVersion("hub-v2");
    await waitForBothIndexed(
      (probe, index) =>
        isHubVersionRendered(probe, "hub-v2") &&
        probe.protocol.ackCount > beforeUpdate[index].protocol.ackCount,
      "hub-v2 is rendered and acknowledged",
    );
    assertWindowGlobalsUnchanged("positive HUB update");

    const beforeRollback = await readBoth();
    const rolledBack = await session.host.rollback(
      "hub-rollback-real-actor",
      session.host.diagnostics.stateRevision,
    );
    await assertOperationSucceeded(rolledBack, "rollback to hub-v1");
    assertPublishedVersion("hub-v1");
    await waitForBothIndexed(
      (probe, index) =>
        isHubVersionRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > beforeRollback[index].protocol.ackCount,
      "hub-v1 rollback is rendered and acknowledged",
    );
    assertWindowGlobalsUnchanged("positive HUB rollback");

    const routeBeforeDisable = await readBoth();
    const disabled = await session.host.disable(
      "hub-disable-real-actor",
      session.host.diagnostics.stateRevision,
    );
    await assertOperationSucceeded(disabled, "disable HUB");
    assertEquals(
      registry.getSnapshotForTests().entries.length,
      0,
      "disable publishes an empty real Actor snapshot",
    );
    await waitForBothIndexed(
      (probe, index) =>
        isHubUnavailable(probe) &&
        probe.protocol.ackCount > routeBeforeDisable[index].protocol.ackCount,
      "unavailable state is rendered and acknowledged",
    );
    assertWindowGlobalsUnchanged("positive HUB disable");

    const beforeReenable = await readBoth();
    const reenabled = await session.host.enable(
      "hub-reenable-real-actor",
      session.host.diagnostics.stateRevision,
    );
    await assertOperationSucceeded(reenabled, "re-enable HUB");
    assertPublishedVersion("hub-v1");
    await waitForBothIndexed(
      (probe, index) =>
        isHubVersionRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > beforeReenable[index].protocol.ackCount,
      "hub-v1 re-enable is rendered and acknowledged",
    );
    assertWindowGlobalsUnchanged("positive HUB re-enable");

    for (const ownedTab of ownedTabs()) {
      const response = await sendHubDomProbeCommand(
        ownedTab,
        "ClickRoute",
        "/overview/home",
      );
      assertEquals(
        response,
        JSON.stringify({ ok: true, route: "/overview/home" }),
        "the fixed probe clicks the real built-in home link",
      );
    }
    await waitForBoth(isHubHome, "built-in home navigation is usable");
    assertWindowGlobalsUnchanged("built-in home link navigation");

    const beforeFinalDisable = await readBoth();
    const disabledAgain = await session.host.disable(
      "hub-disable-real-actor-again",
      session.host.diagnostics.stateRevision,
    );
    await assertOperationSucceeded(disabledAgain, "final HUB disable");
    assertEquals(registry.getSnapshotForTests().entries.length, 0);
    await waitForBothIndexed(
      (probe, index) =>
        probe.dynamicLinks.length === 0 &&
        probe.protocol.ackCount > beforeFinalDisable[index].protocol.ackCount,
      "final disable is rendered and acknowledged in both pages",
    );
    assertWindowGlobalsUnchanged("final positive HUB disable");
  } finally {
    try {
      await clean(session);
    } finally {
      try {
        if (companionTab?.isConnected) gBrowser.removeTab(companionTab);
        if (tab?.isConnected) gBrowser.removeTab(tab);
        await waitForCondition(
          () => registry.diagnosticsForTests.clients === clientBaseline,
          "owned settings HUB clients are removed on tab close",
        );
      } finally {
        if (probeRegistered) {
          ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
        }
      }
    }
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

function openTestBrowserWindow(
  parentWindow?: mozIDOMWindowProxy,
): Window {
  const parent = parentWindow ?? Services.wm.getMostRecentWindow(
    "navigator:browser",
  );
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
        "packaged FeatureHost and discovery share Window and registry identity",
      fn: testPackagedHostAndDiscoveryShareWindowAndRegistryIdentity,
    },
    {
      name: "closing an unrelated tab keeps the live HUB document in W0",
      fn: testClosingUnrelatedTabKeepsHubDocumentLive,
    },
    {
      name:
        "parent actor teardown keeps a live W0 document required until its tab closes",
      fn: testActorTeardownKeepsLiveW0PublicationPending,
    },
    {
      name:
        "owned window close releases pending publication after actor teardown",
      fn: testOwnedWindowCloseCompletesPendingPublicationAfterActorRetires,
    },
    {
      name:
        "real NRFeatureHub Actor and React acknowledge dev/test page publication",
      fn: testRealActorAndReactAcknowledgeHubPublication,
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
