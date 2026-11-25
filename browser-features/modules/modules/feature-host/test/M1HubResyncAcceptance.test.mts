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
  HubDomProbeWindowIdentityModule,
} from "./HubDomProbeTypes.ts";
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
  HUB_CONNECT_BLOCK_QUERY,
  HUB_DOM_PROBE_ACTOR,
  readHubDomProbe,
  registerHubDomProbe,
  sendHubDomProbeCommand,
  waitForHubDomProbe,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/test/HubDomProbeTestUtils.sys.mjs",
) as HubDomProbeTestUtilsModule;

const FEATURE_ID = "m1-actor-acceptance";
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
  version: "hub-v1" | "hub-v2",
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

async function testNeverConnectedAndDisconnectedDocumentsResync(): Promise<
  void
> {
  let tab: HubDomProbeTestTab | null = null;
  let probeRegistered = false;
  let claim: HubHostClaim | null = null;
  const leases: HubGenerationLease[] = [];
  let controller: AbortController | null = null;
  let publication: Promise<unknown> | null = null;
  let publicationSettled = false;
  let publicationFailed = false;
  const primary = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as HubDomProbeTestBrowserWindow | null;
  if (!primary?.gBrowser) throw new Error("the test browser is unavailable");
  const startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
  assert(
    startupMode === "dev" || startupMode === "test",
    "resync acceptance is limited to dev/test startup",
  );
  initializeHubPageDiscovery();
  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  const baseline = registry.diagnosticsForTests;
  try {
    const windowIds: string[] = [];
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      windowIds.push(getFeatureWindowIdentity(windows.getNext() as Window));
    }
    assert(windowIds.length > 0, "the real browser-window set is available");
    const noHubW0 = registry.captureW0(windowIds);
    assertEquals(
      noHubW0.length,
      0,
      "no HUB page exists before this test opens one",
    );
    const claimValue = registry.claimHost(
      FEATURE_ID,
      "m1-actor-acceptance-host",
    );
    claim = claimValue;
    const noHubLease = registry.beginGeneration({
      featureId: claimValue.featureId,
      hostEpoch: claimValue.hostEpoch,
      generation: 1,
    });
    leases.push(noHubLease);
    noHubLease.registerDescriptor(localizedDescriptor("hub-v1"));
    registry.sealGeneration(noHubLease);
    controller = new AbortController();
    publication = registry.publishGeneration(
      noHubLease,
      noHubW0,
      controller.signal,
    );
    await publication;
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      baseline.pendingAcknowledgements,
    );
    assertEquals(
      registry.diagnosticsForTests.pendingTimers,
      baseline.pendingTimers,
    );
    assertEquals(registry.diagnosticsForTests.clients, baseline.clients);
    noHubLease.dispose();
    controller = null;
    publication = null;

    const windowId = getFeatureWindowIdentity(primary);
    registerHubDomProbe();
    probeRegistered = true;
    tab = primary.gBrowser.addTab(
      `${BASE}${HUB_CONNECT_BLOCK_QUERY}#${ROUTE}`,
      {
        inBackground: true,
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectBlocked &&
        probe.protocol.blockedConnectCount > 0,
      "the fixed query marker blocks connect before page subscription",
    );
    const windowGlobal = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    if (!windowGlobal) {
      throw new Error("the HUB page WindowGlobal is unavailable");
    }
    let page = await readHubDomProbe(tab);
    const blockedRequestId = page.protocol.latest.requestId;
    assertEquals(page.protocol.ackCount, 0);
    assertEquals(page.protocol.latest.connectionNonce, null);
    const neverConnectedW0 = registry.captureW0([windowId]);
    assertEquals(neverConnectedW0.length, 1);
    assert(neverConnectedW0[0].isLive());
    const parentClient = neverConnectedW0[0].ensureParentClient();
    assert(
      parentClient !== null,
      "the live never-connected page has a parent client",
    );
    const neverLease = registry.beginGeneration({
      featureId: claimValue.featureId,
      hostEpoch: claimValue.hostEpoch,
      generation: 2,
    });
    leases.push(neverLease);
    neverLease.registerDescriptor(localizedDescriptor("hub-v1"));
    registry.sealGeneration(neverLease);
    controller = new AbortController();
    publicationSettled = false;
    publicationFailed = false;
    publication = registry.publishGeneration(
      neverLease,
      neverConnectedW0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationSettled = true,
      () => {
        publicationSettled = true;
        publicationFailed = true;
      },
    );
    await waitForCondition(
      () =>
        registry.diagnosticsForTests.pendingAcknowledgements >
          baseline.pendingAcknowledgements || publicationSettled,
      "the never-connected live document gates its lease",
    );
    const gateDiagnostics = {
      registry: registry.diagnosticsForTests,
      publicationSettled,
      publicationFailed,
      documentLive: neverConnectedW0[0].isLive(),
      parentClientAvailable: neverConnectedW0[0].ensureParentClient() !== null,
      page: await readHubDomProbe(tab),
    };
    page = gateDiagnostics.page;
    assert(
      gateDiagnostics.registry.pendingAcknowledgements >
          baseline.pendingAcknowledgements && !publicationSettled,
      `never-connected publication must remain pending: ${
        JSON.stringify(gateDiagnostics)
      }`,
    );
    assertEquals(page.protocol.snapshotCount, 0);
    assertEquals(page.protocol.ackCount, 0);
    await sendHubDomProbeCommand(tab, "UnblockConnect");
    await sendHubDomProbeCommand(tab, "Resync");
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        versionIsRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.requestId !== null &&
        probe.protocol.latest.requestId !== blockedRequestId &&
        probe.protocol.latest.connectionNonce !== null,
      "React resync renders and acknowledges the never-connected snapshot",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      windowGlobal,
      "the never-connected resync keeps the same WindowGlobal",
    );
    await waitForCondition(
      () => publicationSettled,
      "never-connected publication completes",
    );
    assert(
      !publicationFailed,
      "React acknowledgement completes the never-connected lease",
    );
    await publication;
    controller = null;
    publication = null;
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      baseline.pendingAcknowledgements,
    );
    assertEquals(
      registry.diagnosticsForTests.pendingTimers,
      baseline.pendingTimers,
    );
    const firstRequestId = page.protocol.latest.requestId;
    const firstNonce = page.protocol.latest.connectionNonce;

    await sendHubDomProbeCommand(tab, "FaultInjectTransportDisconnect");
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.protocol.disconnectCount > 0 &&
        probe.disabledNavigationLabels.some((label) =>
          ["Hotfix sample", "ホットフィックスのサンプル"].includes(label)
        ) && probe.builtInHomeLink?.route === "/overview/home",
      "fault-injected transport disconnect freezes dynamic links but retains home",
    );
    const home = await sendHubDomProbeCommand(
      tab,
      "ClickRoute",
      "/overview/home",
    );
    assert(home !== null, "the fixed home route command was sent");
    await waitForHubDomProbe(
      tab,
      (probe) => probe.hash === "#/overview/home",
      "built-in home remains usable while disconnected",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      windowGlobal,
      "built-in link navigation keeps the same WindowGlobal",
    );

    const disconnectedW0 = registry.captureW0([windowId]);
    assertEquals(disconnectedW0.length, 1);
    assert(disconnectedW0[0].isLive());
    const updateLease = registry.beginGeneration({
      featureId: claimValue.featureId,
      hostEpoch: claimValue.hostEpoch,
      generation: 3,
    });
    leases.push(updateLease);
    updateLease.registerDescriptor(localizedDescriptor("hub-v2"));
    registry.sealGeneration(updateLease);
    controller = new AbortController();
    publicationSettled = false;
    publicationFailed = false;
    publication = registry.publishGeneration(
      updateLease,
      disconnectedW0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationSettled = true,
      () => {
        publicationSettled = true;
        publicationFailed = true;
      },
    );
    await waitForCondition(
      () =>
        registry.diagnosticsForTests.pendingAcknowledgements >
          baseline.pendingAcknowledgements,
      "the disconnected live document remains a publication requirement",
    );
    page = await readHubDomProbe(tab);
    assertEquals(page.hash, "#/overview/home");
    assert(page.disabledNavigationLabels.length > 0);
    assertEquals(publicationSettled, false);
    const ackBeforeResync = page.protocol.ackCount;
    await sendHubDomProbeCommand(tab, "Resync");
    page = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.protocol.latest.requestId !== null &&
        probe.protocol.latest.requestId !== firstRequestId &&
        probe.protocol.latest.connectionNonce !== null &&
        probe.protocol.latest.connectionNonce !== firstNonce &&
        probe.protocol.ackCount > ackBeforeResync,
      "React resync uses a new request and nonce and acknowledges the update",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      windowGlobal,
      "the disconnected resync keeps the same WindowGlobal",
    );
    await waitForCondition(
      () => publicationSettled,
      "disconnected publication completes",
    );
    assert(
      !publicationFailed,
      "React acknowledgement completes the disconnected lease",
    );
    await publication;
    controller = null;
    publication = null;
    const click = await sendHubDomProbeCommand(tab, "ClickRoute", ROUTE);
    assert(click !== null, "the fixed extension route command was sent");
    page = await waitForHubDomProbe(
      tab,
      (probe) => versionIsRendered(probe, "hub-v2"),
      "React renders the updated localized extension view",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      windowGlobal,
      "dynamic link navigation keeps the same WindowGlobal",
    );
  } finally {
    if (tab?.isConnected) primary.gBrowser.removeTab(tab);
    if (controller && !publicationSettled) {
      controller.abort(new Error("M1 resync acceptance test cleanup"));
    }
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
      "resync acceptance cleanup restores registry diagnostic baselines",
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name:
      "never-connected and disconnected HUB documents gate publication and resync through React",
    fn: testNeverConnectedAndDisconnectedDocumentsResync,
  }];
  await runTests("M1HubResyncAcceptance.test.mts", tests);
}
