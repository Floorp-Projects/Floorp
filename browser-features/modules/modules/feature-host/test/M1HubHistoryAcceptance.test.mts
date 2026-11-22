// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { launchDevPorts } from "../../../common/dev-port-offset.ts";
import type { M0TestHostHandle } from "../FeatureHost.sys.mts";
import type { HubPageAcknowledgement } from "../HubPageProtocolTypes.ts";
import type {
  HubDomProbeRead,
  HubDomProbeTestBrowserWindow,
  HubDomProbeTestGBrowser,
  HubDomProbeTestTab,
  HubDomProbeTestUtilsModule,
  HubDomProbeTestWindowGlobal,
  HubDomProbeWindowIdentityModule,
} from "./HubDomProbeTypes.ts";

const { clearTimeout, setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { createM0FeatureHostForTests } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as typeof import("../FeatureHost.sys.mts");
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

const TEST_SETTINGS_BASE = Services.prefs.getBoolPref(
    "nora.dev.allow_http_loader",
    false,
  )
  ? launchDevPorts().httpOrigin("settings")
  : "chrome://noraneko-settings/content/index.html";
const TEST_SETTINGS_URL = `${TEST_SETTINGS_BASE}#/overview/home`;
const HUB_FIXTURE_ROUTE = "/extensions/experimental-local-sample/sample";

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

function isSettingsDocument(
  windowGlobal: HubDomProbeTestWindowGlobal,
): boolean {
  return windowGlobal.documentURI?.spec.startsWith(TEST_SETTINGS_BASE) === true;
}

function isHubVersionRendered(
  probe: HubDomProbeRead,
  version: "hub-v1" | "hub-v2",
): boolean {
  return probe.settingsRootReady &&
    probe.hash === `#${HUB_FIXTURE_ROUTE}` &&
    [
      `Hotfix sample (${version})`,
      `ホットフィックスのサンプル（${version}）`,
    ].includes(probe.messageTitle ?? "") &&
    [
      `The ${version} feature hotfix is active.`,
      `${version} の機能ホットフィックスが有効です。`,
    ].includes(probe.messageBody ?? "") &&
    probe.dynamicLinks.some((link) =>
      link.route === HUB_FIXTURE_ROUTE &&
      ["Hotfix sample", "ホットフィックスのサンプル"].includes(link.label)
    );
}

function acknowledgementFromProbe(
  probe: HubDomProbeRead,
): HubPageAcknowledgement {
  const acknowledgement = probe.protocol.lastAcknowledgement;
  if (
    !acknowledgement.requestId || !acknowledgement.connectionNonce ||
    !acknowledgement.epoch || acknowledgement.revision === null
  ) throw new Error("React acknowledgement metadata is incomplete");
  return {
    type: "snapshot-ack",
    requestId: acknowledgement.requestId,
    connectionNonce: acknowledgement.connectionNonce,
    epoch: acknowledgement.epoch,
    hubRevision: acknowledgement.revision,
  };
}

async function testHistoryBackReconnectsAfterWithheldAcknowledgement(): Promise<
  void
> {
  const mode = Services.prefs.getStringPref("nora.startup.mode", "");
  assert(mode === "dev" || mode === "test", "dev/test mode is required");

  let session: M0TestHostHandle | null = null;
  let primaryWindow: HubDomProbeTestBrowserWindow | null = null;
  let primaryBrowser: HubDomProbeTestGBrowser | null = null;
  let tab: HubDomProbeTestTab | null = null;
  let probeRegistered = false;
  let acknowledgementBlocked = false;
  let closeFaultInjectedCalls = 0;
  let closeObserverCalls = 0;
  let updatePromise:
    | Promise<
      Awaited<ReturnType<M0TestHostHandle["host"]["update"]>>
    >
    | null = null;
  let updateStatus: string | null = null;
  let updateError: unknown = null;
  let updateSettled = false;

  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  assertEquals(initializeHubPageDiscovery(), HubPageDiscoveryService);
  const baseline = { ...registry.diagnosticsForTests };

  try {
    primaryWindow = Services.wm.getMostRecentWindow(
      "navigator:browser",
    ) as HubDomProbeTestBrowserWindow | null;
    if (!primaryWindow?.gBrowser) {
      throw new Error("the primary browser window is unavailable");
    }
    primaryBrowser = primaryWindow.gBrowser;
    const primaryWindowId = getFeatureWindowIdentity(primaryWindow);
    assertEquals(
      registry.captureW0([primaryWindowId]).length,
      0,
      "the primary window has no HUB document before this test",
    );

    registerHubDomProbe();
    probeRegistered = true;
    const systemPrincipal = Services.scriptSecurityManager
      .getSystemPrincipal();
    tab = primaryBrowser.addTab(TEST_SETTINGS_URL, {
      inBackground: false,
      triggeringPrincipal: systemPrincipal,
    });
    await waitForCondition(
      () => {
        const windowGlobal = tab?.linkedBrowser.browsingContext
          .currentWindowGlobal;
        return windowGlobal !== null && windowGlobal !== undefined &&
          isSettingsDocument(windowGlobal);
      },
      "the primary Settings document loads",
    );
    const initialWindowGlobal = tab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!initialWindowGlobal) {
      throw new Error("the primary WindowGlobal is unavailable");
    }
    const initialInnerWindowId = initialWindowGlobal.innerWindowId;
    assert(
      Number.isSafeInteger(initialInnerWindowId),
      "the primary WindowGlobal has an innerWindowId",
    );
    const initialPage = await waitForHubDomProbe(
      tab,
      (probe) =>
        probe.settingsRootReady && probe.pageshow.count > 0 &&
        probe.protocol.connectCount > 0 && probe.protocol.ackCount > 0,
      "the primary page completes its initial React handshake",
    );

    session = await createM0FeatureHostForTests(
      true,
      undefined,
      false,
      undefined,
      { hubPublicationTimeoutMsForTests: 10_000 },
    );
    assertEquals(
      (await session.host.install("m1-hub-history-install", 0, "hub-v1"))
        .status,
      "succeeded",
    );
    assertEquals(
      (await session.host.enable(
        "m1-hub-history-enable",
        session.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    assertEquals(
      await sendHubDomProbeCommand(tab, "ClickRoute", HUB_FIXTURE_ROUTE),
      JSON.stringify({ ok: true, route: HUB_FIXTURE_ROUTE }),
    );
    const v1Page = await waitForHubDomProbe(
      tab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > initialPage.protocol.ackCount &&
        probe.protocol.lastAcknowledgement.requestId !== null,
      "the primary page renders and acknowledges hub-v1",
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      initialWindowGlobal,
      "the v1 render preserves its WindowGlobal",
    );
    assertEquals(initialWindowGlobal.innerWindowId, initialInnerWindowId);
    const initialW0 = registry.captureW0(session.host.windowIdsForTests);
    assertEquals(initialW0.length, 1, "the Settings page is the initial W0");
    const originalDocument = initialW0[0];
    if (!originalDocument) throw new Error("the primary W0 document is absent");

    const originalActor = initialWindowGlobal.getActor("NRFeatureHub");
    const v1Acknowledgement = acknowledgementFromProbe(v1Page);
    const originalConnection = HubPageDiscoveryService.connectActor(
      originalActor,
      initialWindowGlobal,
      v1Acknowledgement.requestId,
    );
    if (!originalConnection) {
      throw new Error("the acknowledged primary connection is absent");
    }
    assertEquals(originalConnection.requestId, v1Acknowledgement.requestId);
    assertEquals(
      originalConnection.connectionNonce,
      v1Acknowledgement.connectionNonce,
    );
    originalConnection.onClosed(() => {
      closeFaultInjectedCalls += 1;
      throw new Error(
        "[M1HubHistoryAcceptance] injected intentional close-listener exception",
      );
    });
    originalConnection.onClosed(() => {
      closeObserverCalls += 1;
    });

    assertEquals(
      await sendHubDomProbeCommand(tab, "BlockReactAcknowledgements"),
      JSON.stringify({ ok: true }),
    );
    acknowledgementBlocked = true;
    updatePromise = session.host.update(
      "m1-hub-history-update-v2",
      session.host.diagnostics.stateRevision,
      "hub-v2",
    );
    void updatePromise.then(
      (result) => {
        updateStatus = result.status;
        updateSettled = true;
      },
      (error: unknown) => {
        updateError = error;
        updateSettled = true;
      },
    );
    const blockedV2Page = await waitForHubDomProbe(
      tab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v2") &&
        probe.protocol.ackBlocked && probe.protocol.blockedAckCount > 0 &&
        probe.protocol.savedAcknowledgementAvailable &&
        probe.protocol.lastAcknowledgement.requestId !== null &&
        probe.protocol.lastAcknowledgement.revision ===
          registry.getSnapshotForTests().hubRevision,
      "React renders v2 and the probe withholds its committed acknowledgement",
    );
    assertEquals(updateSettled, false);
    assertEquals(
      session.host.diagnostics.lastOutcome?.status,
      "committed_pending_publish",
    );
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      baseline.pendingAcknowledgements + 1,
    );
    const withheldAcknowledgement = acknowledgementFromProbe(blockedV2Page);
    assertEquals(
      withheldAcknowledgement.requestId,
      originalConnection.requestId,
    );
    assertEquals(
      withheldAcknowledgement.connectionNonce,
      originalConnection.connectionNonce,
    );
    const currentOriginalConnection = HubPageDiscoveryService.connectActor(
      originalActor,
      initialWindowGlobal,
      withheldAcknowledgement.requestId,
    );
    assertEquals(currentOriginalConnection, originalConnection);
    assertEquals(
      originalConnection.matchesDelivery(
        withheldAcknowledgement.epoch,
        withheldAcknowledgement.hubRevision,
      ),
      true,
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      initialWindowGlobal,
      "the pending v2 render preserves the original WindowGlobal",
    );

    assertEquals(
      await sendHubDomProbeCommand(tab, "UnblockReactAcknowledgements"),
      JSON.stringify({ ok: true }),
    );
    acknowledgementBlocked = false;
    const stillWithheld = await readHubDomProbe(tab);
    assertEquals(stillWithheld.protocol.ackBlocked, false);
    assertEquals(stillWithheld.protocol.savedAcknowledgementAvailable, true);
    assertEquals(updateSettled, false);

    const historyBrowser = tab.linkedBrowser;
    if (!historyBrowser.loadURI || !historyBrowser.webNavigation) {
      throw new Error("the test tab does not expose real session history");
    }
    historyBrowser.loadURI(Services.io.newURI("about:blank"), {
      triggeringPrincipal: systemPrincipal,
    });
    await waitForCondition(
      () => {
        const current = tab?.linkedBrowser.browsingContext.currentWindowGlobal;
        return current !== null && current !== undefined &&
          current !== initialWindowGlobal &&
          current.documentURI?.spec === "about:blank";
      },
      "the tab navigates to a new about:blank WindowGlobal",
    );
    const blankWindowGlobal = tab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!blankWindowGlobal) throw new Error("the blank WindowGlobal is absent");
    assertEquals(blankWindowGlobal === initialWindowGlobal, false);
    assertEquals(originalDocument.isLive(), false);
    assertEquals(
      closeFaultInjectedCalls,
      1,
      "the deliberate close-listener exception is injected on navigation",
    );
    assertEquals(
      closeObserverCalls,
      1,
      "the observer runs after the injected throwing close listener",
    );
    assertEquals(
      registry.captureW0([primaryWindowId]).length,
      0,
      "the old document is no longer a current HUB W0 requirement",
    );
    await waitForCondition(
      () => updateSettled,
      "v2 publication completes when the old page leaves W0",
    );
    if (updateError !== null) throw updateError;
    assertEquals(updateStatus, "succeeded");
    assertEquals(session.host.diagnostics.lastOutcome?.status, "succeeded");
    assertEquals(
      registry.diagnosticsForTests.pendingAcknowledgements,
      baseline.pendingAcknowledgements,
      "navigation releases the withheld publication acknowledgement",
    );
    assertEquals(originalConnection.isClosed(), true);
    assertEquals(historyBrowser.webNavigation.canGoBack, true);

    historyBrowser.webNavigation.goBack();
    await waitForCondition(
      () => {
        const current = tab?.linkedBrowser.browsingContext.currentWindowGlobal;
        return current !== null && current !== undefined &&
          current !== blankWindowGlobal && isSettingsDocument(current);
      },
      "real session-history back restores the Settings URL",
    );
    const restoredWindowGlobal = tab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!restoredWindowGlobal) {
      throw new Error("the restored Settings WindowGlobal is absent");
    }
    const restoredProbe = await waitForHubDomProbe(
      tab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v2") && probe.pageshow.count > 0 &&
        probe.protocol.latest.requestId !== null &&
        probe.protocol.latest.requestId !== withheldAcknowledgement.requestId &&
        probe.protocol.latest.connectionNonce !== null &&
        probe.protocol.latest.connectionNonce !==
          withheldAcknowledgement.connectionNonce &&
        probe.protocol.lastAcknowledgement.requestId ===
          probe.protocol.latest.requestId &&
        probe.protocol.lastAcknowledgement.connectionNonce ===
          probe.protocol.latest.connectionNonce &&
        probe.protocol.lastAcknowledgement.epoch ===
          probe.protocol.latest.epoch &&
        probe.protocol.lastAcknowledgement.revision ===
          probe.protocol.latest.revision,
      "back navigation renders v2 and React acknowledges with a new request and nonce",
    );
    const sameWindowGlobal = restoredWindowGlobal === initialWindowGlobal;
    const sameInnerWindowId = restoredWindowGlobal.innerWindowId ===
      initialInnerWindowId;
    const pageshowPersisted = restoredProbe.pageshow.lastPersisted === true;
    const bfcacheProven = pageshowPersisted ||
      (sameWindowGlobal && sameInnerWindowId);
    Services.console.logStringMessage(
      `[M1HubHistoryAcceptance] ${
        JSON.stringify({
          restoration: bfcacheProven ? "bfcache" : "ordinary-history",
          pageshowPersisted: restoredProbe.pageshow.lastPersisted,
          sameWindowGlobal,
          sameInnerWindowId,
          originalInnerWindowId: initialInnerWindowId,
          restoredInnerWindowId: restoredWindowGlobal.innerWindowId,
        })
      }`,
    );
    assert(isHubVersionRendered(restoredProbe, "hub-v2"));
    const restoredAcknowledgement = acknowledgementFromProbe(restoredProbe);
    assertEquals(
      restoredAcknowledgement.requestId !== withheldAcknowledgement.requestId,
      true,
    );
    assertEquals(
      restoredAcknowledgement.connectionNonce !==
        withheldAcknowledgement.connectionNonce,
      true,
    );
    const restoredSnapshot = registry.getSnapshotForTests();
    const restoredOwnerEntry = restoredSnapshot.entries.find((entry) =>
      entry.route === HUB_FIXTURE_ROUTE
    );
    if (!restoredOwnerEntry) {
      throw new Error("the restored fixture is absent from the HUB snapshot");
    }
    const restoredDocuments = registry.captureW0(
      session.host.windowIdsForTests,
    );
    assertEquals(restoredDocuments.length, 1);
    const restoredDocument = restoredDocuments[0];
    if (!restoredDocument) {
      throw new Error("the restored document is absent from W0");
    }
    assert(restoredDocument.isLive());
    const restoredAckController = new AbortController();
    const restoredAckTimeout = setTimeout(
      () =>
        restoredAckController.abort(
          new Error("restored HUB acknowledgement wait timed out"),
        ),
      9_000,
    );
    try {
      const acceptedSnapshot = await registry.waitForCurrentSnapshot(
        {
          featureId: restoredOwnerEntry.featureId,
          hostEpoch: restoredOwnerEntry.ownerHostEpoch,
          generation: restoredOwnerEntry.ownerGeneration,
        },
        restoredDocuments,
        restoredAckController.signal,
        10_000,
      );
      assertEquals(acceptedSnapshot.epoch, restoredAcknowledgement.epoch);
      assertEquals(
        acceptedSnapshot.hubRevision,
        restoredAcknowledgement.hubRevision,
      );
    } finally {
      clearTimeout(restoredAckTimeout);
    }

    const restoredActor = restoredWindowGlobal.getActor("NRFeatureHub");
    const restoredConnection = HubPageDiscoveryService.connectActor(
      restoredActor,
      restoredWindowGlobal,
      restoredAcknowledgement.requestId,
    );
    if (!restoredConnection) {
      throw new Error("the restored React connection is absent");
    }
    assertEquals(
      restoredConnection.requestId,
      restoredAcknowledgement.requestId,
    );
    assertEquals(
      restoredConnection.connectionNonce,
      restoredAcknowledgement.connectionNonce,
    );
    assertEquals(restoredConnection === originalConnection, false);
    assertEquals(originalConnection.isClosed(), true);
    const staleAcknowledgement = JSON.stringify(withheldAcknowledgement);
    assertEquals(
      HubPageDiscoveryService.handleAcknowledgement(
        restoredActor,
        staleAcknowledgement,
      ),
      false,
      "the current Actor rejects the old genuine React acknowledgement metadata",
    );
    assertEquals(
      HubPageDiscoveryService.handleAcknowledgement(
        originalActor,
        staleAcknowledgement,
      ),
      false,
      "the old Actor connection cannot accept its stale acknowledgement",
    );
    assertEquals(
      HubPageDiscoveryService.connectActor(
        restoredActor,
        restoredWindowGlobal,
        restoredAcknowledgement.requestId,
      ),
      restoredConnection,
      "stale acknowledgement checks preserve the restored connection",
    );
    const afterStaleAcknowledgement = await readHubDomProbe(tab);
    assert(isHubVersionRendered(afterStaleAcknowledgement, "hub-v2"));
    assertEquals(
      afterStaleAcknowledgement.protocol.lastAcknowledgement.requestId,
      restoredAcknowledgement.requestId,
    );
    assertEquals(
      afterStaleAcknowledgement.protocol.lastAcknowledgement.connectionNonce,
      restoredAcknowledgement.connectionNonce,
    );
    assertEquals(
      tab.linkedBrowser.browsingContext.currentWindowGlobal,
      restoredWindowGlobal,
      "back navigation and the new real React acknowledgement preserve the restored WindowGlobal",
    );
  } finally {
    if (tab && acknowledgementBlocked) {
      await sendHubDomProbeCommand(
        tab,
        "UnblockReactAcknowledgements",
      ).catch(() => {});
    }
    try {
      if (session) {
        try {
          const report = await session.shutdownForRestartForTests();
          assertEquals(report.timedOut, false);
          assertEquals(report.errors.length, 0);
        } finally {
          await session.cleanup();
        }
      }
    } finally {
      try {
        if (tab?.isConnected && primaryBrowser) primaryBrowser.removeTab(tab);
      } finally {
        try {
          if (probeRegistered) {
            ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
          }
        } finally {
          if (updatePromise) await updatePromise.catch(() => {});
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
                diagnostics.stagedEntries === baseline.stagedEntries &&
                (!session ||
                  (session.host.windowLifecycleDiagnosticsForTests.tracked ===
                      0 &&
                    session.host.windowLifecycleDiagnosticsForTests
                        .admissionTasks === 0 &&
                    session.host.windowLifecycleDiagnosticsForTests
                        .pendingStartup === 0 &&
                    session.host.windowLifecycleDiagnosticsForTests
                        .readyWaiters === 0 &&
                    session.host.windowLifecycleDiagnosticsForTests
                        .pendingDetaches === 0 &&
                    session.host.windowLifecycleDiagnosticsForTests
                        .cleanupFailures === 0 &&
                    !session.host.windowLifecycleDiagnosticsForTests
                      .observers &&
                    session.host.runningScopeDiagnosticsForTests.resources ===
                      0 &&
                    session.host.runningScopeDiagnosticsForTests.childScopes ===
                      0 &&
                    session.host.runningScopeDiagnosticsForTests.tasks === 0 &&
                    session.host.runningScopeDiagnosticsForTests
                        .pendingTasks === 0));
            },
            "history acceptance cleanup restores registry and lifecycle baselines",
          );
          if (primaryWindow) assertEquals(primaryWindow.closed, false);
        }
      }
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name:
      "history back restores the real HUB after navigation releases a withheld v2 acknowledgement",
    fn: testHistoryBackReconnectsAfterWithheldAcknowledgement,
  }];
  await runTests("M1HubHistoryAcceptance.test.mts", tests);
}
