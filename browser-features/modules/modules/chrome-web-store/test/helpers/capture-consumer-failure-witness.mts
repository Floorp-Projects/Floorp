// SPDX-License-Identifier: MPL-2.0
// Observe only after the original failure, before its existing cleanup.
import type {
  CaptureFailureTab,
  CaptureFailureTracker,
  CaptureFailureWindow,
} from "./capture-consumer-failure-witness-types.ts";

function read<T>(fn: () => T): T | { readError: string } {
  try {
    return fn();
  } catch (error) {
    return { readError: String(error) };
  }
}

export function captureConsumerFailure(
  rawWindow: unknown,
  rawTabs: unknown[],
  error: unknown,
): void {
  const owner = rawWindow as CaptureFailureWindow;
  const tabs = rawTabs as CaptureFailureTab[];
  const { BrowserWindowTracker } = ChromeUtils.importESModule(
    "resource:///modules/BrowserWindowTracker.sys.mjs",
  ) as { BrowserWindowTracker: CaptureFailureTracker };
  const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ) as { PrivateBrowsingUtils: { isWindowPrivate(window: Window): boolean } };
  const windows = BrowserWindowTracker.orderedWindows;
  const windowState = (window: CaptureFailureWindow) => ({
    owner: window === owner,
    closed: window.closed,
    windowType: window.document?.documentElement?.getAttribute("windowtype") ??
      null,
    privateBrowsing: read(() => PrivateBrowsingUtils.isWindowPrivate(window)),
    innerWidth: window.innerWidth,
    innerHeight: window.innerHeight,
    outerWidth: window.outerWidth,
    outerHeight: window.outerHeight,
    devicePixelRatio: window.devicePixelRatio,
    windowState: window.windowState,
    documentVisibility: window.document?.visibilityState ?? null,
    selectedFixtureTab: tabs.indexOf(window.gBrowser.selectedTab),
  });
  const report = {
    diagnosticOnly: true,
    phase: "after-original-failure-before-original-finally",
    timestamp: Date.now(),
    originalError: String(error),
    runtime: {
      version: Services.appinfo.version,
      buildId: Services.appinfo.appBuildID,
      os: Services.appinfo.OS,
    },
    owner: read(() => ({
      ...windowState(owner),
      tracked: windows.includes(owner),
    })),
    trackedWindowCount: windows.length,
    trackedWindowsTruncated: windows.length > 8,
    trackedWindows: windows.slice(0, 8).map((window) =>
      read(() => windowState(window))
    ),
    visibleSources: read(() => BrowserWindowTracker.getAllVisibleTabs()),
    fixtureTabCount: tabs.length,
    fixtureTabsTruncated: tabs.length > 3,
    tabs: tabs.slice(0, 3).map((tab) =>
      read(() => {
        const browser = tab.linkedBrowser;
        const global = browser.browsingContext.currentWindowGlobal;
        const rect = browser.getBoundingClientRect();
        return {
          selected: tab === owner.gBrowser.selectedTab,
          selectedProperty: tab.selected,
          hidden: tab.hidden,
          isConnected: tab.isConnected,
          linkedPanel: tab.linkedPanel,
          inVisibleTabs: owner.gBrowser.visibleTabs.includes(tab),
          workspaceId: tab.getAttribute("floorpWorkspaceId"),
          userContextId: tab.getAttribute("usercontextid"),
          browserId: browser.browserId,
          browsingContextBrowserId: browser.browsingContext.browserId,
          browsingContextId: browser.browsingContext.id,
          url: browser.currentURI.spec,
          title: browser.contentTitle,
          remoteType: browser.remoteType,
          innerWindowId: global?.innerWindowId ?? null,
          documentURI: global?.documentURI.spec ?? null,
          principalOrigin: global?.documentPrincipal.originNoSuffix ?? null,
          rect: {
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height,
          },
        };
      })
    ),
    interpretation:
      "Read only after failure; this does not establish window geometry or source-list state before native allocation.",
  };
  console.error(
    "[CaptureConsumerNativeFailureWitness]",
    JSON.stringify(report),
  );
}
