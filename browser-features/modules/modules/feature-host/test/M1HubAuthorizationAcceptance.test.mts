// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert as harnessAssert,
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { launchDevPorts } from "../../../common/dev-port-offset.ts";
import type {
  HubDomProbeActorLookup,
  HubDomProbeRead,
  HubDomProbeTestBrowserWindow,
  HubDomProbeTestTab,
  HubDomProbeTestUtilsModule,
  HubDomProbeTestWindowGlobal,
  HubDomProbeWindowIdentityModule,
} from "./HubDomProbeTypes.ts";
import type { HubGenerationLease, HubHostClaim } from "../HubRegistryTypes.ts";

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { HubPageDiscoveryService, initializeHubPageDiscovery } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/feature-host/HubPageDiscovery.sys.mjs",
  ) as typeof import("../HubPageDiscovery.sys.mts");
const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);
const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);
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

const FEATURE_ID = "m1-actor-authorization";
const LOCAL_ID = "sample";
const ROUTE = `/extensions/${FEATURE_ID}/${LOCAL_ID}`;
const PORTS = launchDevPorts();
const SETTINGS_ORIGIN = PORTS.httpOrigin("settings");
const WRONG_PORT_ORIGIN = PORTS.httpOrigin("newtab");
const SETTINGS_URL = Services.prefs.getBoolPref(
    "nora.dev.allow_http_loader",
    false,
  )
  ? SETTINGS_ORIGIN
  : "chrome://noraneko-settings/content/index.html";
const SETTINGS_PAGE_URL = `${SETTINGS_URL}#/overview/home`;
const CHROME_SETTINGS_ORIGIN = "chrome://noraneko-settings";
const wrongOriginURL = new URL(SETTINGS_ORIGIN);
wrongOriginURL.hostname = `${wrongOriginURL.hostname}.`;
const WRONG_ORIGIN = wrongOriginURL.origin;
const allowedAliasURL = new URL(SETTINGS_ORIGIN);
allowedAliasURL.hostname = "127.0.0.1";
const ALLOWED_ALIAS_ORIGIN = allowedAliasURL.origin;
const ALLOW_HTTP = Services.prefs.getBoolPref(
  "nora.dev.allow_http_loader",
  false,
);

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

function openBrowserWindow(
  privateBrowsing: boolean,
): HubDomProbeTestBrowserWindow {
  const args = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString,
  );
  args.data = "about:blank";
  return BrowserWindowTracker.openWindow({
    args,
    features: "width=640,height=480",
    remote: true,
    fission: true,
    private: privateBrowsing,
  }) as HubDomProbeTestBrowserWindow;
}

async function waitForBrowserStartup(
  window: HubDomProbeTestBrowserWindow,
  label: string,
): Promise<void> {
  await waitForCondition(
    () =>
      !window.closed && window.gBrowserInit?.delayedStartupFinished === true &&
      window.gBrowser !== undefined,
    label,
    15_000,
  );
}

function addTab(
  browser: NonNullable<HubDomProbeTestBrowserWindow["gBrowser"]>,
  url: string,
): HubDomProbeTestTab {
  const tab = browser.addTab(url, {
    inBackground: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  browser.selectedTab = tab;
  return tab;
}

function hasOrigin(
  manager: HubDomProbeTestWindowGlobal | null,
  expectedOrigin: string,
): boolean {
  if (!manager || manager.isClosed || !manager.documentURI) return false;
  try {
    const actual = new URL(manager.documentURI.spec);
    const expected = new URL(expectedOrigin);
    return actual.protocol === expected.protocol &&
      actual.host === expected.host;
  } catch {
    return false;
  }
}

async function waitForDocumentAtOrigin(
  tab: HubDomProbeTestTab,
  origin: string,
  label: string,
): Promise<HubDomProbeTestWindowGlobal> {
  await waitForCondition(
    () =>
      hasOrigin(
        tab.linkedBrowser.browsingContext.currentWindowGlobal,
        origin,
      ),
    `${label} loads a document at ${origin}`,
  );
  const manager = tab.linkedBrowser.browsingContext.currentWindowGlobal;
  if (!manager || !hasOrigin(manager, origin)) {
    throw new Error(`${label} did not load at ${origin}`);
  }
  assert(
    !manager.documentURI?.spec.startsWith("about:neterror"),
    `${label} loaded a document rather than a network error`,
  );
  return manager;
}

function getHubActor(
  manager: HubDomProbeTestWindowGlobal,
): HubDomProbeActorLookup {
  try {
    return {
      actor: manager.getActor("NRFeatureHub") ?? null,
      registrationError: null,
    };
  } catch (error) {
    return { actor: null, registrationError: String(error) };
  }
}

function localizedDescriptor(): string {
  return JSON.stringify({
    localId: LOCAL_ID,
    route: ROUTE,
    titleKey: "hotfixHub.sample.title",
    navigation: { labelKey: "hotfixHub.sample.navigation", order: 1 },
    view: {
      kind: "message",
      titleKey: "hotfixHub.sample.title",
      bodyKey: "hotfixHub.sample.body",
      params: { version: "m1-authorization-v1" },
    },
  });
}

function localizedViewRendered(probe: HubDomProbeRead): boolean {
  return probe.hash === `#${ROUTE}` &&
    [
      "Hotfix sample (m1-authorization-v1)",
      "ホットフィックスのサンプル（m1-authorization-v1）",
    ].includes(probe.messageTitle ?? "") &&
    [
      "The m1-authorization-v1 feature hotfix is active.",
      "m1-authorization-v1 の機能ホットフィックスが有効です。",
    ].includes(probe.messageBody ?? "");
}

async function testHubAuthorizationBoundaries(): Promise<void> {
  const startupMode = Services.prefs.getStringPref("nora.startup.mode", "");
  assert(
    startupMode === "dev" || startupMode === "test",
    "authorization acceptance is limited to dev/test startup",
  );
  const primaryWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as HubDomProbeTestBrowserWindow | null;
  if (!primaryWindow?.gBrowser) {
    throw new Error("the harness browser window is unavailable");
  }

  const registry = HubPageDiscoveryService.sharedRegistryForTests();
  initializeHubPageDiscovery();
  const baseline = registry.diagnosticsForTests;
  const actorBoundaries: string[] = [];
  let ownedWindow: HubDomProbeTestBrowserWindow | null = null;
  let privateWindow: HubDomProbeTestBrowserWindow | null = null;
  let positiveTab: HubDomProbeTestTab | null = null;
  const ownedTabs: HubDomProbeTestTab[] = [];
  let probeRegistered = false;
  let frameCreated = false;
  let claim: HubHostClaim | null = null;
  let lease: HubGenerationLease | null = null;
  let controller: AbortController | null = null;
  let publication: Promise<unknown> | null = null;
  let publicationSettled = false;
  let bodyFailed = false;
  let bodyFailure: unknown;
  let cleanupFailure: AggregateError | null = null;

  try {
    registerHubDomProbe();
    probeRegistered = true;
    ownedWindow = openBrowserWindow(false);
    assert(
      ownedWindow !== primaryWindow,
      "the test window is owned, not primary",
    );
    await waitForBrowserStartup(ownedWindow, "the owned normal window starts");
    const browser = ownedWindow.gBrowser;
    if (!browser) throw new Error("the owned normal window has no gBrowser");
    const windowId = getFeatureWindowIdentity(ownedWindow);

    positiveTab = addTab(browser, SETTINGS_PAGE_URL);
    ownedTabs.push(positiveTab);
    const positiveManager = await waitForDocumentAtOrigin(
      positiveTab,
      ALLOW_HTTP ? SETTINGS_ORIGIN : CHROME_SETTINGS_ORIGIN,
      "the positive settings page",
    );
    const initialPage = await waitForHubDomProbe(
      positiveTab,
      (probe) =>
        probe.settingsRootReady && probe.protocol.connectCount > 0 &&
        probe.protocol.ackCount > 0 &&
        probe.protocol.latest.requestId !== null,
      "the positive top-level settings Actor connects and acknowledges",
    );
    const positiveActor = getHubActor(positiveManager);
    assert(
      positiveActor.actor !== null,
      `the positive settings WindowGlobal has NRFeatureHub: ${positiveActor.registrationError}`,
    );
    const w0 = registry.captureW0([windowId]);
    assertEquals(
      w0.length,
      1,
      "the positive page is the owned window's only W0 document",
    );
    assert(w0[0].isLive(), "the positive settings W0 document is live");
    const clientCountWithPositivePage = baseline.clients + 1;
    assertEquals(
      registry.diagnosticsForTests.clients,
      clientCountWithPositivePage,
      "the normal settings page establishes one parent client",
    );

    claim = registry.claimHost(FEATURE_ID, "m1-authorization-host");
    lease = registry.beginGeneration({
      featureId: claim.featureId,
      hostEpoch: claim.hostEpoch,
      generation: 1,
    });
    lease.registerDescriptor(localizedDescriptor());
    registry.sealGeneration(lease);
    controller = new AbortController();
    publication = registry.publishGeneration(
      lease,
      w0,
      controller.signal,
      10_000,
    );
    void publication.then(
      () => publicationSettled = true,
      () => publicationSettled = true,
    );
    await publication;
    controller = null;
    publication = null;

    const beforeRouteClick = await readHubDomProbe(positiveTab);
    assertEquals(
      await sendHubDomProbeCommand(positiveTab, "ClickRoute", ROUTE),
      JSON.stringify({ ok: true, route: ROUTE }),
      "the fixed probe navigates to the published route",
    );
    const rendered = await waitForHubDomProbe(
      positiveTab,
      (probe) =>
        localizedViewRendered(probe) &&
        probe.protocol.ackCount > beforeRouteClick.protocol.ackCount,
      "the positive settings page renders and acknowledges the localized view",
    );
    assertEquals(
      positiveTab.linkedBrowser.browsingContext.currentWindowGlobal,
      positiveManager,
      "positive settings navigation retains its WindowGlobal",
    );
    const activeRequestId = initialPage.protocol.latest.requestId;
    assert(activeRequestId !== null, "the positive request ID is captured");

    if (ALLOW_HTTP) {
      const aliasTab = addTab(
        browser,
        `${ALLOWED_ALIAS_ORIGIN}#/overview/home`,
      );
      ownedTabs.push(aliasTab);
      const aliasManager = await waitForDocumentAtOrigin(
        aliasTab,
        ALLOWED_ALIAS_ORIGIN,
        "explicitly allowed 127.0.0.1 Settings alias",
      );
      const aliasActor = getHubActor(aliasManager);
      assert(
        aliasActor.actor !== null,
        `allowed alias WindowGlobal has its own NRFeatureHub Actor: ${aliasActor.registrationError}`,
      );
      if (!aliasActor.actor) {
        throw new Error("allowed alias WindowGlobal has no NRFeatureHub Actor");
      }
      const aliasConnection = HubPageDiscoveryService.connectActor(
        aliasActor.actor,
        aliasManager,
        "m1-auth-allowed-alias",
      );
      assert(
        aliasConnection !== null,
        "the real allowed-alias Actor passes the parent Settings-origin guard",
      );
      actorBoundaries.push(
        "allowed 127.0.0.1 alias: actual WindowGlobal Actor accepted",
      );
      assertEquals(
        registry.captureW0([windowId]).length,
        2,
        "discovery includes the canonical page and allowed alias",
      );
      assertEquals(
        registry.diagnosticsForTests.clients,
        baseline.clients + 2,
        "the allowed alias can connect as a second parent client",
      );
      browser.removeTab(aliasTab);
      await waitForCondition(
        () =>
          registry.captureW0([windowId]).length === 1 &&
          registry.diagnosticsForTests.clients === clientCountWithPositivePage,
        "closing the allowed alias restores the canonical client baseline",
      );
    }

    const assertRejectedDocument = (
      label: string,
      manager: HubDomProbeTestWindowGlobal,
      actorLookup: HubDomProbeActorLookup,
      targetWindowId: string,
      expectedW0Count: number,
      requireActor: boolean,
      requestId: string,
    ): void => {
      if (requireActor) {
        assert(
          actorLookup.actor !== null,
          `${label}: expected this matching document to have its own Actor (${actorLookup.registrationError})`,
        );
      }
      if (!actorLookup.actor) {
        actorBoundaries.push(
          `${label}: registration boundary (${
            actorLookup.registrationError ?? "getActor returned null"
          })`,
        );
      }
      const connection = actorLookup.actor
        ? HubPageDiscoveryService.connectActor(
          actorLookup.actor,
          manager,
          requestId,
        )
        : null;
      if (actorLookup.actor) {
        actorBoundaries.push(`${label}: actual document Actor was available`);
      }
      const discovered = registry.captureW0([targetWindowId]);
      const diagnostics = registry.diagnosticsForTests;
      assert(
        connection === null && discovered.length === expectedW0Count &&
          diagnostics.clients === clientCountWithPositivePage,
        `${label} is rejected and excluded from discovery: ${
          JSON.stringify({
            documentURI: manager.documentURI?.spec,
            actorAvailable: actorLookup.actor !== null,
            registrationError: actorLookup.registrationError,
            returnedConnection: connection
              ? {
                requestId: connection.requestId,
                connectionNonce: connection.connectionNonce,
              }
              : null,
            discoveredDocuments: discovered.length,
            clients: diagnostics.clients,
          })
        }`,
      );
    };

    const wrongPortTab = addTab(
      browser,
      `${WRONG_PORT_ORIGIN}/?m1-hub-authorization-wrong-port=1`,
    );
    ownedTabs.push(wrongPortTab);
    const wrongPortManager = await waitForDocumentAtOrigin(
      wrongPortTab,
      WRONG_PORT_ORIGIN,
      "wrong-port local dev page",
    );
    assertRejectedDocument(
      "wrong loopback port",
      wrongPortManager,
      getHubActor(wrongPortManager),
      windowId,
      1,
      ALLOW_HTTP,
      "m1-auth-wrong-port",
    );

    const wrongOriginTab = addTab(
      browser,
      `${WRONG_ORIGIN}#/overview/home`,
    );
    ownedTabs.push(wrongOriginTab);
    const wrongOriginManager = await waitForDocumentAtOrigin(
      wrongOriginTab,
      WRONG_ORIGIN,
      "same-port unlisted hostname",
    );
    assertRejectedDocument(
      "wrong origin",
      wrongOriginManager,
      getHubActor(wrongOriginManager),
      windowId,
      1,
      false,
      "m1-auth-wrong-origin",
    );

    assertEquals(
      await sendHubDomProbeCommand(positiveTab, "CreateSameOriginFrame"),
      JSON.stringify({ ok: true }),
      "the fixed probe creates one same-origin iframe",
    );
    frameCreated = true;
    await waitForCondition(
      () =>
        positiveTab!.linkedBrowser.browsingContext.children.some((context) =>
          context.currentWindowGlobal?.documentURI?.spec.includes(
            "m1-hub-authorization-frame=1",
          ) && hasOrigin(
            context.currentWindowGlobal,
            ALLOW_HTTP ? SETTINGS_ORIGIN : CHROME_SETTINGS_ORIGIN,
          )
        ),
      "the owned same-origin iframe loads",
    );
    const frameContext = positiveTab.linkedBrowser.browsingContext.children
      .find((context) =>
        context.currentWindowGlobal?.documentURI?.spec.includes(
          "m1-hub-authorization-frame=1",
        )
      );
    const frameManager = frameContext?.currentWindowGlobal;
    if (!frameManager) {
      throw new Error("the same-origin iframe WindowGlobal is absent");
    }
    const frameActor = getHubActor(frameManager);
    assertRejectedDocument(
      "same-origin iframe",
      frameManager,
      frameActor,
      windowId,
      1,
      false,
      "m1-auth-frame",
    );

    privateWindow = openBrowserWindow(true);
    assert(
      privateWindow !== primaryWindow && privateWindow !== ownedWindow,
      "the private test window is separately owned",
    );
    await waitForBrowserStartup(
      privateWindow,
      "the owned private window starts",
    );
    assert(
      PrivateBrowsingUtils.isWindowPrivate(privateWindow),
      "the owned window is private before authorization is tested",
    );
    const privateWindowId = getFeatureWindowIdentity(privateWindow);
    const privateBrowser = privateWindow.gBrowser;
    if (!privateBrowser) throw new Error("the private window has no gBrowser");
    const privateTab = addTab(
      privateBrowser,
      SETTINGS_PAGE_URL,
    );
    const privateManager = await waitForDocumentAtOrigin(
      privateTab,
      ALLOW_HTTP ? SETTINGS_ORIGIN : CHROME_SETTINGS_ORIGIN,
      "private settings page",
    );
    assertRejectedDocument(
      "private window",
      privateManager,
      getHubActor(privateManager),
      privateWindowId,
      0,
      false,
      "m1-auth-private-window",
    );

    if (!ALLOW_HTTP) {
      const packagedHttpTab = addTab(
        browser,
        `${SETTINGS_ORIGIN}#/overview/home`,
      );
      ownedTabs.push(packagedHttpTab);
      const packagedHttpManager = await waitForDocumentAtOrigin(
        packagedHttpTab,
        SETTINGS_ORIGIN,
        "correct-port HTTP document with loader disabled",
      );
      const packagedHttpActor = getHubActor(packagedHttpManager);
      assert(
        !packagedHttpManager.isClosed,
        "the loader-off HTTP WindowGlobal remains live",
      );
      assertEquals(
        packagedHttpTab.linkedBrowser.browsingContext.currentWindowGlobal,
        packagedHttpManager,
        "the loader-off HTTP document is the current WindowGlobal",
      );
      assertEquals(
        packagedHttpManager.browsingContext.parent,
        null,
        "the loader-off HTTP document is top-level",
      );
      assertEquals(
        packagedHttpActor.actor,
        null,
        "the loader-off Settings origin does not receive NRFeatureHub",
      );
      assert(
        typeof packagedHttpActor.registrationError === "string" &&
          packagedHttpActor.registrationError.trim().length > 0,
        "the loader-off document is rejected at a genuine Actor registration boundary",
      );
      assertRejectedDocument(
        "HTTP-loader-off correct settings port",
        packagedHttpManager,
        packagedHttpActor,
        windowId,
        1,
        false,
        "m1-auth-http-loader-off",
      );
      actorBoundaries.push(
        "HTTP-loader-off correct settings port: Actor registration denied before parent discovery",
      );
    }

    const finalPage = await readHubDomProbe(positiveTab);
    assertEquals(finalPage.protocol.latest.requestId, activeRequestId);
    assertEquals(
      finalPage.protocol.latest.connectionNonce,
      rendered.protocol.latest.connectionNonce,
    );
    assertEquals(
      positiveTab.linkedBrowser.browsingContext.currentWindowGlobal,
      positiveManager,
      "authorization probes preserve the positive settings WindowGlobal",
    );
    Services.console.logStringMessage(
      `[M1HubAuthorizationAcceptance] ${
        JSON.stringify({
          allowHttpLoader: ALLOW_HTTP,
          settingsOrigin: SETTINGS_ORIGIN,
          wrongPortOrigin: WRONG_PORT_ORIGIN,
          wrongOrigin: WRONG_ORIGIN,
          actorBoundaries,
        })
      }`,
    );
  } catch (error) {
    bodyFailed = true;
    bodyFailure = error;
  } finally {
    const cleanupFailures: Array<{ label: string; error: unknown }> = [];
    const attemptCleanup = async (
      label: string,
      action: () => unknown | Promise<unknown>,
    ): Promise<void> => {
      try {
        await action();
      } catch (error) {
        cleanupFailures.push({ label, error });
        console.error(
          `[M1HubAuthorizationAcceptance] cleanup failed (${label})`,
          error,
        );
      }
    };
    const cleanupController = controller;
    const cleanupAbortReason = new Error(
      "M1 authorization acceptance cleanup",
    );

    await attemptCleanup("remove same-origin frame", async () => {
      if (frameCreated && positiveTab?.isConnected) {
        await sendHubDomProbeCommand(
          positiveTab,
          "RemoveSameOriginFrame",
        );
      }
    });
    await attemptCleanup("abort pending publication", () => {
      if (cleanupController && !publicationSettled) {
        cleanupController.abort(cleanupAbortReason);
      }
    });
    await attemptCleanup("settle publication", async () => {
      if (!publication) return;
      try {
        await publication;
      } catch (error) {
        if (error !== cleanupAbortReason) throw error;
      }
    });
    await attemptCleanup("dispose generation lease", () => lease?.dispose());
    await attemptCleanup("release host claim", () => claim?.release());
    await attemptCleanup("close private window", () => {
      if (
        privateWindow && privateWindow !== primaryWindow &&
        !privateWindow.closed
      ) privateWindow.close();
    });
    await attemptCleanup("close normal window", () => {
      if (ownedWindow && ownedWindow !== primaryWindow && !ownedWindow.closed) {
        ownedWindow.close();
      }
    });
    await attemptCleanup("unregister probe Actor", () => {
      if (probeRegistered) {
        ChromeUtils.unregisterWindowActor(HUB_DOM_PROBE_ACTOR);
      }
    });
    await attemptCleanup("preserve harness primary window", () => {
      assert(!primaryWindow.closed, "the harness primary window remains open");
    });
    await attemptCleanup("restore registry diagnostics", () =>
      waitForCondition(
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
        "authorization cleanup restores registry diagnostics",
      ));

    if (cleanupFailures.length > 0) {
      const failures: unknown[] = [];
      for (const { label, error } of cleanupFailures) {
        failures.push(new AggregateError([error], `cleanup failed (${label})`));
      }
      cleanupFailure = new AggregateError(
        failures,
        `M1 authorization cleanup failed: ${
          cleanupFailures.map(({ label, error }) =>
            `${label}: ${String(error)}`
          ).join("; ")
        }`,
      );
    }
  }
  if (bodyFailed || cleanupFailure) {
    const failures: unknown[] = bodyFailed ? [bodyFailure] : [];
    if (cleanupFailure) failures.push(cleanupFailure);
    throw new AggregateError(
      failures,
      `M1 authorization acceptance or cleanup failed: ${
        failures.map(String).join("; ")
      }`,
    );
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [{
    name:
      "real settings HUB authorization rejects wrong origins, private pages, and frames",
    fn: testHubAuthorizationBoundaries,
  }];
  await runTests("M1HubAuthorizationAcceptance.test.mts", tests);
}
