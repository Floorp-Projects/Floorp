// SPDX-License-Identifier: MPL-2.0

import type {
  AppLifecycleCapabilities,
  AppLifecycleSnapshot,
  AppQuitPlan,
  AppQuitRequest,
} from "./appLifecycleTypes.ts";

export const KEEP_WEB_APPS_RUNNING_PREF =
  "floorp.browser.nativeApp.keepRunningAfterBrowserQuit";
export const DEFAULT_KEEP_WEB_APPS_RUNNING = true;
export const APP_LIFECYCLE_PROTOCOL_VERSION = 1;

export function supportsAppLifecycle(
  capabilities: AppLifecycleCapabilities | null,
): boolean {
  return capabilities !== null &&
    capabilities.protocolVersion === APP_LIFECYCLE_PROTOCOL_VERSION &&
    capabilities.sharedProfile === true &&
    capabilities.nativeWindowOwnership === true &&
    capabilities.independentAppQuit === true &&
    capabilities.backgroundSession === true;
}

/**
 * Computes intent without closing windows or mutating the snapshot. The native
 * adapter must preserve beforeunload cancellation, retain the shared runtime
 * during close, and obtain a new snapshot before releasing its survival area.
 * Restart/logout remain global and must never be converted into browser-only
 * close. This function does not create, copy, or open another profile.
 */
export function planAppQuit(
  request: AppQuitRequest,
  snapshot: AppLifecycleSnapshot,
  keepRunningAfterBrowserQuit: boolean,
  capabilities: AppLifecycleCapabilities | null,
): AppQuitPlan {
  if (request.kind === "runtime") {
    return { kind: "quit-runtime", reason: request.reason };
  }
  if (!supportsAppLifecycle(capabilities)) {
    return { kind: "unhandled" };
  }

  if (request.kind === "browser") {
    const hasWebApps = snapshot.windows.some((win) => win.kind === "web-app") ||
      snapshot.pendingLaunches.length > 0;
    if (!keepRunningAfterBrowserQuit || !hasWebApps) {
      return { kind: "quit-runtime", reason: "quit-all" };
    }
    const windowIds = snapshot.windows
      .filter((win) => win.kind === "browser")
      .map((win) => win.id);
    if (windowIds.length === 0) {
      return { kind: "noop" };
    }
    return {
      kind: "close-windows",
      windowIds,
      cancelLaunchIds: [],
      retainRuntimeAfterClose: true,
    };
  }

  const windowIds = snapshot.windows
    .filter((win) => win.kind === "web-app" && win.appId === request.appId)
    .map((win) => win.id);
  const cancelLaunchIds = snapshot.pendingLaunches
    .filter((launch) => launch.appId === request.appId)
    .map((launch) => launch.id);
  if (windowIds.length === 0 && cancelLaunchIds.length === 0) {
    return { kind: "noop" };
  }
  const hasOtherWindows = snapshot.windows.some((win) =>
    win.kind === "browser" || win.appId !== request.appId
  );
  const hasOtherLaunches = snapshot.pendingLaunches.some((launch) =>
    launch.appId !== request.appId
  );
  return {
    kind: "close-windows",
    windowIds,
    cancelLaunchIds,
    retainRuntimeAfterClose: hasOtherWindows || hasOtherLaunches,
  };
}
