// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import type {
  HubDomProbeRead,
  HubDomProbeTestBrowserWindow,
  HubDomProbeTestTab,
  HubDomProbeTestUtilsModule,
  HubDomProbeTestWindowGlobal,
  HubDomProbeWindowIdentityModule,
} from "./HubDomProbeTypes.ts";
import type { HubPageAcknowledgement } from "../HubPageProtocolTypes.ts";
import type { HubGenerationLease, HubHostClaim } from "../HubRegistryTypes.ts";
import { launchDevPorts } from "../../../common/dev-port-offset.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { HubPageDiscoveryService, initializeHubPageDiscovery } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/HubPageDiscovery.sys.mjs",
  ) as typeof import("../HubPageDiscovery.sys.mts");
const { getFeatureWindowIdentity } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/WindowIdentity.sys.mjs",
) as HubDomProbeWindowIdentityModule;
const {
  HUB_DOM_PROBE_ACTOR,
  readHubDomProbe,
  registerHubDomProbe,
  sendHubDomProbeCommand,
  waitForHubDomProbe,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/test/HubDomProbeTestUtils.sys.mjs",
) as HubDomProbeTestUtilsModule;

const FEATURE_ID = "m1-actor-fencing";
const LOCAL_ID = "sample";
const ROUTE = `/extensions/${FEATURE_ID}/${LOCAL_ID}`;
const SETTINGS_ORIGIN = launchDevPorts().httpOrigin("settings");
const BASE = Services.prefs.getBoolPref("nora.dev.allow_http_loader", false)
  ? SETTINGS_ORIGIN
  : "chrome://noraneko-settings/content/index.html";

function assert(condition: boolean, message = "assertion failed"): void {
  harnessAssert(condition, message);
}

function assertEquals<T>(
  actual: T,
  expected: T,
  message = "assertion failed",
): void {
  harnessAssertEquals(actual, expected, message);
}

async function waitForCondition(
  condition: () => boolean,
  label: string,
  timeoutMs = 10_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (condition()) return;
    await new Promise<void>((resolve) => setTimeout(resolve, 20));
  }
  throw new Error(`${label} timed out`);
}

function localizedDescriptor(version: string): string {
  return JSON.stringify({
    localId: LOCAL_ID,
    route: ROUTE,
    titleKey: "hotfixHub.sample.title",
    navigation: { labelKey: "hotfixHub.sample.navigation", order: 1 },
    view: {
      kind: "message",
      titleKey: "hotfixHub.sample.title",
      bodyKey: "hotfixHub.sample.body",
      params: { version },
    },
  });
}

function versionIsRendered(
  probe: HubDomProbeRead,
  version: "m1-fencing-v1" | "m1-fencing-v2",
): boolean {
  return probe.hash === `#${ROUTE}` &&
    [
      `Hotfix sample (${version})`,
      `ホットフィックスのサンプル（${version}）`,
    ].includes(probe.messageTitle ?? "") &&
    [
      `The ${version} feature hotfix is active.`,
      `${version} の機能ホットフィックスが有効です。`,
    ].includes(probe.messageBody ?? "") &&
    probe.dynamicLinks.some((link) => link.route === ROUTE);
}

function assertWindowGlobal(
  tab: HubDomProbeTestTab,
  expected: HubDomProbeTestWindowGlobal,
  label: string,
): void {
  assertEquals(
    tab.linkedBrowser.browsingContext.currentWindowGlobal,
    expected,
    label,
  );
}

async function testActorAcknowledgementFencingAndReplay(): Promise<void> {
  let tab: HubDomProbeTestTab | null = null;
  let probeRegistered = false;
  let claim: HubHostClaim | null = null;
  const leases: HubGenerationLease[] = [];
  let controller: AbortController | null = null;
  let publication: Promise<unknown> | null = null;
  let publicationSettled = false;
  let publicationError: string | null = null;
  const mainWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as HubDomProbeTestBrowserWindow | null;
  if (!mainWindow?.gBrowser) throw new Error("the test browser is unavailable");
  const startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
  assert(
    startupMode === "dev" || startupMode === "test",
    "acknowledgement fencing is limited to dev/test startup",
  );
  const gBrowser = mainWindow.gBrowser;
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  initializeHubPageDiscovery();
  const baseline = registry.diagnosticsForTests;
  try {
    const windowId = getFeatureWindowIdentity(mainWindow);
    registerHubDomProbe();
    probeRegistered = true;
    tab = gBrowser.addTab(`${BASE}#/overview/home`, {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager
        .getSystemPrincipal(),
    });
    const readyPage = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.requestId !== null &&
        probe.protocol.latest.connectionNonce !== null &&
        probe.protocol.latest.epoch !== null &&
        probe.protocol.latest.revision !== null,
      "the real settings Actor completes its initial connection and snapshot ack",
    );
    const activeTab = tab;
    const windowGlobal = activeTab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!windowGlobal) {
      throw new Error("the settings WindowGlobal is unavailable");
    }
    const actor = windowGlobal.getActor("NRFeatureHub");
    const firstRequestId = readyPage.protocol.latest.requestId;
    const firstNonce = readyPage.protocol.latest.connectionNonce;
    const firstEpoch = readyPage.protocol.latest.epoch;
    const firstRevision = readyPage.protocol.latest.revision;
    if (
      !firstRequestId || !firstNonce || !firstEpoch || firstRevision === null
    ) {
      throw new Error(
        "the initial React acknowledgement metadata is incomplete",
      );
    }
    const firstConnection = HubPageDiscoveryService.connectActor(
      actor,
      windowGlobal,
      firstRequestId,
    );
    if (!firstConnection) {
      throw new Error("the initial parent connection is absent");
    }
    assertEquals(firstConnection.requestId, firstRequestId);
    assertEquals(firstConnection.connectionNonce, firstNonce);

    const w0 = registry.captureW0([windowId]);
    assertEquals(w0.length, 1, "the real settings page is in W0");
    assert(w0[0].isLive(), "the W0 document is live");
    claim = registry.claimHost(FEATURE_ID, "m1-actor-fencing-host");
    const firstLease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    leases.push(firstLease);
    firstLease.registerDescriptor(localizedDescriptor("m1-fencing-v1"));
    registry.sealGeneration(firstLease);
    controller = new AbortController();
    publicationSettled = false;
    publicationError = null;
    publication = registry.publishGeneration(
      firstLease,
      w0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationSettled = true,
      (error) => {
        publicationSettled = true;
        publicationError = String(error);
      },
    );
    await publication;
    controller = null;
    publication = null;
    assertEquals(publicationError, null);
    assertWindowGlobal(
      tab,
      windowGlobal,
      "initial publication preserves the WindowGlobal",
    );

    const beforeRouteClick = await readHubDomProbe(tab);
    const routeClick = await sendHubDomProbeCommand(tab, "ClickRoute", ROUTE);
    assertEquals(routeClick, JSON.stringify({ ok: true, route: ROUTE }));
    let page = await waitForHubDomProbe(
      tab,
      (probe) =>
        versionIsRendered(probe, "m1-fencing-v1") &&
        probe.protocol.ackCount > beforeRouteClick.protocol.ackCount,
      "React renders and acknowledges the initial localized route",
    );
    assertWindowGlobal(
      tab,
      windowGlobal,
      "dynamic link navigation preserves the WindowGlobal",
    );

    const beforeResync = page;
    const resyncResult = await sendHubDomProbeCommand(tab, "Resync");
    assertEquals(resyncResult, JSON.stringify({ ok: true }));
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.protocol.latest.requestId !== null &&
        probe.protocol.latest.requestId !== firstRequestId &&
        probe.protocol.latest.connectionNonce !== null &&
        probe.protocol.latest.connectionNonce !== firstNonce &&
        probe.protocol.ackCount > beforeResync.protocol.ackCount,
      "normal React resync establishes a new request and nonce",
    );
    assertWindowGlobal(tab, windowGlobal, "resync preserves the WindowGlobal");
    const resyncedRequestId = page.protocol.latest.requestId;
    const resyncedNonce = page.protocol.latest.connectionNonce;
    const oldRevision = page.protocol.latest.revision;
    const currentEpoch = page.protocol.latest.epoch;
    if (
      !resyncedRequestId || !resyncedNonce || oldRevision === null ||
      !currentEpoch
    ) throw new Error("the resynced acknowledgement metadata is incomplete");
    const resyncedConnection = HubPageDiscoveryService.connectActor(
      actor,
      windowGlobal,
      resyncedRequestId,
    );
    if (!resyncedConnection) {
      throw new Error("the resynced parent connection is absent");
    }
    assertEquals(resyncedConnection.requestId, resyncedRequestId);
    assertEquals(resyncedConnection.connectionNonce, resyncedNonce);

    const blockResult = await sendHubDomProbeCommand(
      tab,
      "BlockReactAcknowledgements",
    );
    assertEquals(blockResult, JSON.stringify({ ok: true }));
    const beforeUpdate = await readHubDomProbe(tab);
    const updateLease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 2,
    });
    leases.push(updateLease);
    updateLease.registerDescriptor(localizedDescriptor("m1-fencing-v2"));
    registry.sealGeneration(updateLease);
    controller = new AbortController();
    publicationSettled = false;
    publicationError = null;
    publication = registry.publishGeneration(
      updateLease,
      w0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationSettled = true,
      (error) => {
        publicationSettled = true;
        publicationError = String(error);
      },
    );
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        versionIsRendered(probe, "m1-fencing-v2") &&
        probe.protocol.ackCount > beforeUpdate.protocol.ackCount &&
        probe.protocol.blockedAckCount > 0 &&
        probe.protocol.ackBlocked &&
        probe.protocol.savedAcknowledgementAvailable,
      "the probe blocks and saves the genuine committed React acknowledgement",
    );
    assertWindowGlobal(
      tab,
      windowGlobal,
      "blocked snapshot rendering preserves the WindowGlobal",
    );
    const pendingDiagnostics = registry.diagnosticsForTests;
    assert(
      pendingDiagnostics.pendingAcknowledgements >
          baseline.pendingAcknowledgements && !publicationSettled,
      `the blocked React acknowledgement keeps publication pending: ${
        JSON.stringify({ pendingDiagnostics, publicationError, page })
      }`,
    );

    const activeConnection = HubPageDiscoveryService.connectActor(
      actor,
      windowGlobal,
      resyncedRequestId,
    );
    if (!activeConnection) {
      throw new Error("the active parent connection is absent");
    }
    assertEquals(activeConnection, resyncedConnection);
    const blockedPage = page;
    const pendingClientCount = pendingDiagnostics.clients;
    const revision = page.protocol.latest.revision;
    const epoch = page.protocol.latest.epoch;
    if (revision === null || !epoch) {
      throw new Error(
        "the blocked React acknowledgement metadata is incomplete",
      );
    }
    assert(
      revision > oldRevision,
      "the blocked ack belongs to a later revision",
    );
    assertEquals(activeConnection.requestId, resyncedRequestId);
    assertEquals(activeConnection.connectionNonce, resyncedNonce);
    const validAck: HubPageAcknowledgement = {
      type: "snapshot-ack",
      connectionNonce: resyncedNonce,
      requestId: resyncedRequestId,
      epoch,
      hubRevision: revision,
    };
    const validAckJson = JSON.stringify(validAck);

    const assertPublicationStillPending = async (
      label: string,
    ): Promise<void> => {
      await new Promise<void>((resolve) => setTimeout(resolve, 20));
      const diagnostics = registry.diagnosticsForTests;
      const current = HubPageDiscoveryService.connectActor(
        actor,
        windowGlobal,
        resyncedRequestId,
      );
      assertEquals(
        current,
        activeConnection,
        `${label}: direct parser fault injection preserves the current Actor connection`,
      );
      assertEquals(
        diagnostics.clients,
        pendingClientCount,
        `${label}: client count is unchanged`,
      );
      assert(
        diagnostics.pendingAcknowledgements > baseline.pendingAcknowledgements,
        `${label}: direct parser fault injection leaves the publication waiter pending`,
      );
      const currentPage = await readHubDomProbe(activeTab);
      assertEquals(
        currentPage.protocol.latest.requestId,
        blockedPage.protocol.latest.requestId,
        `${label}: request ID is unchanged`,
      );
      assertEquals(
        currentPage.protocol.latest.connectionNonce,
        blockedPage.protocol.latest.connectionNonce,
        `${label}: nonce is unchanged`,
      );
      assertEquals(
        currentPage.protocol.latest.epoch,
        blockedPage.protocol.latest.epoch,
        `${label}: epoch is unchanged`,
      );
      assertEquals(
        currentPage.protocol.latest.revision,
        blockedPage.protocol.latest.revision,
        `${label}: revision is unchanged`,
      );
      assertEquals(
        currentPage.protocol.connectCount,
        blockedPage.protocol.connectCount,
        `${label}: no page reconnect occurred`,
      );
      assertEquals(
        currentPage.protocol.disconnectCount,
        blockedPage.protocol.disconnectCount,
        `${label}: no page disconnect occurred`,
      );
      assertEquals(
        currentPage.protocol.snapshotCount,
        blockedPage.protocol.snapshotCount,
        `${label}: no new snapshot was delivered`,
      );
      assertEquals(
        currentPage.protocol.ackCount,
        blockedPage.protocol.ackCount,
        `${label}: no new page acknowledgement occurred`,
      );
      assertEquals(
        publicationSettled,
        false,
        `${label}: publication remains pending`,
      );
      assertEquals(
        publicationError,
        null,
        `${label}: publication has no failure`,
      );
    };
    const injectRejectedAcknowledgement = async (
      label: string,
      data: unknown,
    ): Promise<void> => {
      const accepted = HubPageDiscoveryService.handleAcknowledgement(
        actor,
        data,
      );
      assertEquals(
        accepted,
        false,
        `${label}: parent parser fault injection rejects`,
      );
      await assertPublicationStillPending(label);
    };

    await injectRejectedAcknowledgement(
      "stale request",
      JSON.stringify({ ...validAck, requestId: firstRequestId }),
    );
    await injectRejectedAcknowledgement(
      "stale nonce",
      JSON.stringify({ ...validAck, connectionNonce: firstNonce }),
    );
    await injectRejectedAcknowledgement(
      "stale epoch",
      JSON.stringify({ ...validAck, epoch: `${epoch}-stale` }),
    );
    await injectRejectedAcknowledgement(
      "old revision",
      JSON.stringify({ ...validAck, hubRevision: oldRevision }),
    );
    await injectRejectedAcknowledgement(
      "future revision",
      JSON.stringify({ ...validAck, hubRevision: revision + 1 }),
    );
    await injectRejectedAcknowledgement("malformed JSON", "{invalid");
    await injectRejectedAcknowledgement(
      "unsupported acknowledgement kind",
      JSON.stringify({ ...validAck, type: "unsupported" }),
    );
    const oversizedAck = `${validAckJson}${" ".repeat(4097)}`;
    assert(oversizedAck.length > 4096);
    assertEquals(oversizedAck.slice(0, validAckJson.length), validAckJson);
    await injectRejectedAcknowledgement(
      "oversized otherwise-valid acknowledgement",
      oversizedAck,
    );
    await injectRejectedAcknowledgement(
      "unknown acknowledgement key",
      JSON.stringify({ ...validAck, unknownKey: "probe" }),
    );
    const prototypeLikeAck = JSON.stringify({
      ...validAck,
      ["__proto__"]: { probe: true },
    });
    assert(
      prototypeLikeAck.includes('"__proto__"'),
      "the prototype-like key is serialized",
    );
    await injectRejectedAcknowledgement(
      "prototype-like acknowledgement key",
      prototypeLikeAck,
    );
    for (
      const [label, hubRevision] of [
        ["negative revision", -1],
        ["fractional revision", 1.5],
        ["unsafe revision", Number.MAX_SAFE_INTEGER + 1],
        ["string revision", "1"],
      ] as const
    ) {
      await injectRejectedAcknowledgement(
        `invalid numeric field (${label})`,
        JSON.stringify({ ...validAck, hubRevision }),
      );
    }

    for (
      const [label, requestId] of [
        ["null", null],
        ["number", 17],
        ["empty string", ""],
        ["overlong string", "r".repeat(81)],
        ["object", { requestId: resyncedRequestId }],
      ] as const
    ) {
      const result = HubPageDiscoveryService.connectActor(
        actor,
        windowGlobal,
        requestId,
      );
      assertEquals(
        result,
        null,
        `invalid connect request (${label}) is rejected`,
      );
      await assertPublicationStillPending(`invalid connect request (${label})`);
    }
    const duplicateConnection = HubPageDiscoveryService.connectActor(
      actor,
      windowGlobal,
      resyncedRequestId,
    );
    assertEquals(
      duplicateConnection,
      activeConnection,
      "duplicate connect retains one connection",
    );
    await assertPublicationStillPending("duplicate connect");

    HubPageDiscoveryService.disconnectRequest(
      actor,
      JSON.stringify({
        requestId: firstRequestId,
        connectionNonce: firstNonce,
      }),
    );
    await assertPublicationStillPending("late old disconnect");
    HubPageDiscoveryService.disconnectRequest(
      actor,
      JSON.stringify({ requestId: firstRequestId, connectionNonce: null }),
    );
    await assertPublicationStillPending("late pre-nonce cancellation");

    const beforeReplay = page.protocol.ackCount;
    const replayResult = await sendHubDomProbeCommand(
      tab,
      "ReplaySavedReactAcknowledgement",
    );
    assertEquals(replayResult, JSON.stringify({ ok: true }));
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.protocol.ackCount > beforeReplay &&
        !probe.protocol.ackBlocked &&
        !probe.protocol.savedAcknowledgementAvailable,
      "replaying the saved acknowledgement reaches the real Actor listener",
    );
    assertWindowGlobal(
      tab,
      windowGlobal,
      "genuine acknowledgement replay preserves the WindowGlobal",
    );
    assertEquals(page.protocol.latest.requestId, resyncedRequestId);
    assertEquals(page.protocol.latest.connectionNonce, resyncedNonce);
    await waitForCondition(
      () => publicationSettled,
      "real IPC acknowledgement completes the blocked publication",
    );
    assertEquals(
      publicationError,
      null,
      "the genuine replay completes publication",
    );
    await publication;
    controller = null;
    publication = null;
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      baseline.pendingAcknowledgements,
    );
  } finally {
    if (controller && !publicationSettled) {
      controller.abort(new Error("M1 acknowledgement fencing cleanup"));
    }
    if (tab?.isConnected) gBrowser.removeTab(tab);
    await publication?.catch(() => {});
    for (const lease of leases) lease.dispose();
    claim?.release();
    if (probeRegistered) ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
    await waitForCondition(
      () => {
        const diagnostics = registry.diagnosticsForTests;
        return diagnostics.clients === baseline.clients &&
          diagnostics.pendingAcknowledgements ===
            baseline.pendingAcknowledgements &&
          diagnostics.pendingTimers === baseline.pendingTimers &&
          diagnostics.claims === baseline.claims &&
          diagnostics.leases === baseline.leases &&
          diagnostics.publishedEntries === baseline.publishedEntries &&
          diagnostics.stagedEntries === baseline.stagedEntries;
      },
      "acknowledgement fencing cleanup restores registry diagnostics",
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name: "real Actor acknowledgements enforce request and revision fences",
    fn: testActorAcknowledgementFencingAndReplay,
  }];
  await runTests("M1HubAckFencingAcceptance.test.mts", tests);
}
