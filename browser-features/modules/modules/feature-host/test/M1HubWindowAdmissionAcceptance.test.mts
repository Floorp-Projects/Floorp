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
import type {
  HubDomProbeRead,
  HubDomProbeTestBrowserWindow,
  HubDomProbeTestGBrowser,
  HubDomProbeTestTab,
  HubDomProbeTestUtilsModule,
  HubDomProbeTestWindowGlobal,
  HubDomProbeWindowIdentityModule,
} from "./HubDomProbeTypes.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { createM0FeatureHostForTests } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/FeatureHost.sys.mjs",
) as typeof import("../FeatureHost.sys.mts");
const { HubPageDiscoveryService, initializeHubPageDiscovery } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/HubPageDiscovery.sys.mjs",
  ) as typeof import("../HubPageDiscovery.sys.mts");
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);
const { getFeatureWindowIdentity } = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/WindowIdentity.sys.mjs",
) as HubDomProbeWindowIdentityModule;
const {
  HUB_ACK_BLOCK_QUERY,
  HUB_DOM_PROBE_ACTOR,
  readHubDomProbe,
  registerHubDomProbe,
  sendHubDomProbeCommand,
  waitForHubDomProbe,
} = ChromeUtils.importESModule(
  "resource://noraneko/modules/feature-host/test/HubDomProbeTestUtils.sys.mjs",
) as HubDomProbeTestUtilsModule;
const TEST_SETTINGS_ORIGIN = launchDevPorts().httpOrigin("settings");
const TEST_SETTINGS_BASE = Services.prefs.getBoolPref(
    "nora.dev.allow_http_loader",
    false,
  )
  ? TEST_SETTINGS_ORIGIN
  : "chrome://noraneko-settings/content/index.html";
const HUB_FIXTURE_ROUTE = "/extensions/experimental-local-sample/sample";
const TEST_SETTINGS_URL = `${TEST_SETTINGS_BASE}#/overview/home`;

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
  timeoutMs = 8_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (condition()) return;
    await new Promise<void>((resolve) => setTimeout(resolve, 10));
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

function openTestBrowserWindow(
  parent: HubDomProbeTestBrowserWindow,
  initialURL: string,
): HubDomProbeTestBrowserWindow {
  const args = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  args.data = initialURL;
  return Services.ww.openWindow(
    parent as unknown as mozIDOMWindowProxy,
    "chrome://browser/content/browser.xhtml",
    "_blank",
    "chrome,all,dialog=no,width=640,height=480",
    args,
  ) as unknown as HubDomProbeTestBrowserWindow;
}

async function testNewWindowAdmissionWaitsForItsReactAcknowledgement(): Promise<
  void
> {
  const mode = Services.prefs.getStringPref("nora.startup.mode", "");
  assert(mode === "dev" || mode === "test", "dev/test mode is required");

  let session: M0TestHostHandle | null = null;
  let primaryWindow: HubDomProbeTestBrowserWindow | null = null;
  let primaryBrowser: HubDomProbeTestGBrowser | null = null;
  let primaryTab: HubDomProbeTestTab | null = null;
  let secondWindow: HubDomProbeTestBrowserWindow | null = null;
  let secondTab: HubDomProbeTestTab | null = null;
  let probeRegistered = false;
  let updatePromise:
    | Promise<
      Awaited<ReturnType<M0TestHostHandle["host"]["update"]>>
    >
    | null = null;
  let updateStatus: string | null = null;
  let updateError: unknown = null;
  let updateSettled = false;
  let primaryAcknowledgementBlocked = false;
  let secondAcknowledgementBlocked = false;

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
    primaryTab = primaryBrowser.addTab(TEST_SETTINGS_URL, {
      inBackground: false,
      triggeringPrincipal: systemPrincipal,
    });
    await waitForCondition(
      () => {
        const windowGlobal = primaryTab?.linkedBrowser.browsingContext
          .currentWindowGlobal;
        return windowGlobal !== null && windowGlobal !== undefined &&
          isSettingsDocument(windowGlobal);
      },
      "primary Settings document loads",
    );
    const primaryWindowGlobal = primaryTab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!primaryWindowGlobal) {
      throw new Error("primary WindowGlobal unavailable");
    }
    const primaryInitialProbe = await waitForHubDomProbe(
      primaryTab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackCount > 0,
      "primary Settings page connects and acknowledges",
    );
    assertEquals(
      primaryWindowGlobal.documentURI?.spec.startsWith(TEST_SETTINGS_BASE),
      true,
    );
    assertEquals(registry.captureW0([primaryWindowId]).length, 1);

    session = await createM0FeatureHostForTests(
      true,
      undefined,
      false,
      undefined,
      { hubPublicationTimeoutMsForTests: 10_000 },
    );
    assertEquals(
      (await session.host.install("m1-window-admission-install", 0, "hub-v1"))
        .status,
      "succeeded",
    );
    assertEquals(
      (await session.host.enable(
        "m1-window-admission-enable",
        session.host.diagnostics.stateRevision,
      )).status,
      "succeeded",
    );
    const primaryRouteClick = await sendHubDomProbeCommand(
      primaryTab,
      "ClickRoute",
      HUB_FIXTURE_ROUTE,
    );
    assertEquals(
      primaryRouteClick,
      JSON.stringify({ ok: true, route: HUB_FIXTURE_ROUTE }),
    );
    await waitForHubDomProbe(
      primaryTab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v1") &&
        probe.protocol.ackCount > primaryInitialProbe.protocol.ackCount,
      "primary HUB renders and acknowledges hub-v1",
    );
    assertEquals(
      primaryTab.linkedBrowser.browsingContext.currentWindowGlobal,
      primaryWindowGlobal,
    );
    const initialW0 = registry.captureW0(session.host.windowIdsForTests);
    assertEquals(
      initialW0.length,
      1,
      "initial W0 contains the primary HUB only",
    );
    assertEquals(initialW0[0]?.windowId, primaryWindowId);

    const blockPrimary = await sendHubDomProbeCommand(
      primaryTab,
      "BlockReactAcknowledgements",
    );
    assertEquals(blockPrimary, JSON.stringify({ ok: true }));
    primaryAcknowledgementBlocked = true;
    const blockAckProbe = await readHubDomProbe(primaryTab);
    assertEquals(blockAckProbe.protocol.ackBlocked, true);

    updatePromise = session.host.update(
      "m1-window-admission-update-v2",
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
    const primaryV2Blocked = await waitForHubDomProbe(
      primaryTab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v2") &&
        probe.protocol.ackBlocked && probe.protocol.blockedAckCount > 0 &&
        probe.protocol.savedAcknowledgementAvailable &&
        probe.protocol.latest.revision ===
          registry.getSnapshotForTests().hubRevision,
      "primary React renders hub-v2 and saves its genuine acknowledgement",
    );
    assertEquals(updateSettled, false);
    assertEquals(
      session.host.diagnostics.lastOutcome?.status,
      "committed_pending_publish",
    );
    assertEquals(
      registry.getSnapshotForTests().entries[0]?.view.params?.version,
      "hub-v2",
    );
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    assertEquals(
      primaryV2Blocked.protocol.latest.epoch,
      registry.getSnapshotForTests().epoch,
    );
    assertEquals(
      primaryTab.linkedBrowser.browsingContext.currentWindowGlobal,
      primaryWindowGlobal,
      "primary WindowGlobal survives the pending update",
    );

    const trackedBaseline = session.host.windowLifecycleDiagnosticsForTests
      .tracked;
    const secondURL =
      `${TEST_SETTINGS_BASE}${HUB_ACK_BLOCK_QUERY}#${HUB_FIXTURE_ROUTE}`;
    secondWindow = openTestBrowserWindow(primaryWindow, secondURL);
    assert(
      secondWindow !== primaryWindow,
      "second normal window is separately owned",
    );
    assertEquals(PrivateBrowsingUtils.isWindowPrivate(secondWindow), false);
    await waitForCondition(
      () =>
        secondWindow?.gBrowserInit?.delayedStartupFinished === true &&
        secondWindow.gBrowser !== undefined,
      "second browser window finishes startup",
    );
    if (!secondWindow.gBrowser) throw new Error("second gBrowser unavailable");
    secondTab = secondWindow.gBrowser.selectedTab;
    await waitForCondition(
      () => {
        const windowGlobal = secondTab?.linkedBrowser.browsingContext
          .currentWindowGlobal;
        return windowGlobal !== null && windowGlobal !== undefined &&
          isSettingsDocument(windowGlobal);
      },
      "second window's initial HUB Settings document loads",
    );
    const secondWindowGlobal = secondTab.linkedBrowser.browsingContext
      .currentWindowGlobal;
    if (!secondWindowGlobal) throw new Error("second WindowGlobal unavailable");
    const secondInitialProbe = await waitForHubDomProbe(
      secondTab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackBlocked,
      "second page probe blocks acknowledgements from Actor creation",
    );
    secondAcknowledgementBlocked = true;
    const secondV2Blocked = await waitForHubDomProbe(
      secondTab,
      (probe) =>
        isHubVersionRendered(probe, "hub-v2") &&
        probe.protocol.ackBlocked && probe.protocol.blockedAckCount > 0 &&
        probe.protocol.savedAcknowledgementAvailable &&
        probe.protocol.latest.revision ===
          registry.getSnapshotForTests().hubRevision,
      "new window renders hub-v2 while the original acknowledgement is held",
    );
    assert(secondInitialProbe.protocol.ackBlocked);
    assertEquals(updateSettled, false);
    assertEquals(registry.diagnosticsForTests.pendingAcknowledgements, 1);
    assertEquals(
      secondTab.linkedBrowser.browsingContext.currentWindowGlobal,
      secondWindowGlobal,
      "second WindowGlobal survives its initial v2 render",
    );

    const secondWindowId = getFeatureWindowIdentity(secondWindow);
    await waitForCondition(
      () =>
        session?.host.windowIdForTests(secondWindow!) === secondWindowId &&
        session.host.windowLifecycleDiagnosticsForTests.tracked ===
          trackedBaseline + 1,
      "FeatureHost tracks the actual second window identity",
    );
    assertEquals(session.host.windowIdForTests(secondWindow), secondWindowId);
    assertEquals(secondWindowId === primaryWindowId, false);

    const replayPrimary = await sendHubDomProbeCommand(
      primaryTab,
      "ReplaySavedReactAcknowledgement",
    );
    assertEquals(replayPrimary, JSON.stringify({ ok: true }));
    primaryAcknowledgementBlocked = false;
    await waitForCondition(
      () => updateSettled,
      "hub-v2 update completes after primary ack",
    );
    if (updateError !== null) throw updateError;
    assertEquals(updateStatus, "succeeded");
    assertEquals(session.host.diagnostics.lastOutcome?.status, "succeeded");
    assertEquals(secondV2Blocked.protocol.ackBlocked, true);
    const secondStillBlocked = await readHubDomProbe(secondTab);
    assertEquals(secondStillBlocked.protocol.ackBlocked, true);
    assertEquals(
      secondStillBlocked.protocol.savedAcknowledgementAvailable,
      true,
    );
    await waitForCondition(
      () =>
        session?.host.windowLifecycleDiagnosticsForTests.admissionTasks ===
          1 &&
        session.host.windowIdsForTests.includes(secondWindowId) &&
        registry.diagnosticsForTests.pendingAcknowledgements === 1,
      "second window has a separate pending readiness acknowledgement",
    );
    assertEquals(session.host.windowIdForTests(secondWindow), secondWindowId);
    assertEquals(
      primaryTab.linkedBrowser.browsingContext.currentWindowGlobal,
      primaryWindowGlobal,
      "primary WindowGlobal survives successful update",
    );
    assertEquals(
      secondTab.linkedBrowser.browsingContext.currentWindowGlobal,
      secondWindowGlobal,
      "second WindowGlobal survives while admission is pending",
    );

    const replaySecond = await sendHubDomProbeCommand(
      secondTab,
      "ReplaySavedReactAcknowledgement",
    );
    assertEquals(replaySecond, JSON.stringify({ ok: true }));
    secondAcknowledgementBlocked = false;
    await waitForCondition(
      () =>
        session?.host.windowLifecycleDiagnosticsForTests.admissionTasks ===
          0 && registry.diagnosticsForTests.pendingAcknowledgements === 0,
      "second window admission completes after its genuine React ack",
    );
    const finalPrimaryProbe = await readHubDomProbe(primaryTab);
    const finalSecondProbe = await readHubDomProbe(secondTab);
    assert(isHubVersionRendered(finalPrimaryProbe, "hub-v2"));
    assert(isHubVersionRendered(finalSecondProbe, "hub-v2"));
    assertEquals(
      primaryTab.linkedBrowser.browsingContext.currentWindowGlobal,
      primaryWindowGlobal,
    );
    assertEquals(
      secondTab.linkedBrowser.browsingContext.currentWindowGlobal,
      secondWindowGlobal,
    );
  } finally {
    if (primaryTab && primaryAcknowledgementBlocked) {
      await sendHubDomProbeCommand(
        primaryTab,
        "ReplaySavedReactAcknowledgement",
      ).catch(() => {});
    }
    if (secondTab && secondAcknowledgementBlocked) {
      await sendHubDomProbeCommand(
        secondTab,
        "ReplaySavedReactAcknowledgement",
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
        if (secondWindow && !secondWindow.closed) {
          try {
            if (secondTab?.isConnected && secondWindow.gBrowser) {
              secondWindow.gBrowser.removeTab(secondTab);
            }
          } finally {
            if (!secondWindow.closed) secondWindow.close();
          }
        }
      } finally {
        try {
          if (primaryTab?.isConnected && primaryBrowser) {
            primaryBrowser.removeTab(primaryTab);
          }
        } finally {
          try {
            if (probeRegistered) {
              ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
            }
          } finally {
            if (primaryWindow) assertEquals(primaryWindow.closed, false);
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
                  diagnostics.publishedEntries ===
                    baseline.publishedEntries &&
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
                      session.host.runningScopeDiagnosticsForTests
                          .childScopes ===
                        0 &&
                      session.host.runningScopeDiagnosticsForTests.tasks ===
                        0 &&
                      session.host.runningScopeDiagnosticsForTests
                          .pendingTasks === 0));
              },
              "window admission cleanup restores registry and lifecycle baselines",
            );
          }
        }
      }
    }
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name:
      "a real second HUB window waits for its own React acknowledgement after v2 publication",
    fn: testNewWindowAdmissionWaitsForItsReactAcknowledgement,
  }];
  await runTests("M1HubWindowAdmissionAcceptance.test.mts", tests);
}
