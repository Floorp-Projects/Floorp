// SPDX-License-Identifier: MPL-2.0

import Workspaces from "#features-chrome/common/workspaces";
import type { WorkspacesService } from "#features-chrome/common/workspaces/workspacesService.ts";

type ChromeWindow = Window & { gBrowser?: GBrowser };

interface PaletteBrowser {
  contentPrincipal?: nsIPrincipal;
  browsingContext?: {
    originAttributes?: Record<string, unknown>;
  };
  loadURI?: (
    uri: nsIURI,
    options?: Record<string, unknown>,
  ) => void;
}

export function isPaletteTargetAvailable(
  target: PaletteTargetContext,
): boolean {
  if (target.window.closed || !target.tab.isConnected) {
    return false;
  }

  const currentGBrowser = (target.window as ChromeWindow).gBrowser;
  if (currentGBrowser !== target.gBrowser) {
    return false;
  }

  try {
    if (currentGBrowser.getBrowserForTab(target.tab) !== target.browser) {
      return false;
    }
    const currentWorkspaces = Workspaces.getCtx(target.window);
    return currentWorkspaces === target.workspaces &&
      (currentWorkspaces?.getCurrentWorkspaceUserContextId() ?? 0) ===
        target.workspaceUserContextId;
  } catch {
    return false;
  }
}

export interface PaletteTargetContext {
  window: Window;
  gBrowser: GBrowser;
  tab: XULElement;
  browser: PaletteBrowser;
  principal: nsIPrincipal;
  originAttributes: Record<string, unknown>;
  workspaces: WorkspacesService | null;
  workspaceUserContextId: number;
}

export function resolvePaletteTarget(
  targetWindow: Window,
): PaletteTargetContext | null {
  const gBrowser = (targetWindow as ChromeWindow).gBrowser;
  const tab = gBrowser?.selectedTab;
  if (!gBrowser || !tab) {
    return null;
  }

  const browser = gBrowser.getBrowserForTab(tab) as PaletteBrowser | undefined;
  const principal = browser?.contentPrincipal;
  const originAttributes = browser?.browsingContext?.originAttributes;
  if (!browser || !principal || !originAttributes) {
    return null;
  }

  const workspaces = Workspaces.getCtx(targetWindow);
  return {
    window: targetWindow,
    gBrowser,
    tab,
    browser,
    principal,
    originAttributes: { ...originAttributes },
    workspaces,
    workspaceUserContextId: workspaces?.getCurrentWorkspaceUserContextId() ?? 0,
  };
}

export function createTriggeringPrincipal(
  target: PaletteTargetContext,
  userContextId: number,
): nsIPrincipal {
  if (!Number.isSafeInteger(userContextId) || userContextId < 0) {
    throw new TypeError("userContextId must be a non-negative integer");
  }

  const originAttributes = {
    ...target.originAttributes,
    userContextId,
  };
  const securityManager = Services.scriptSecurityManager;

  if (target.principal.isContentPrincipal) {
    return securityManager.principalWithOA(
      target.principal,
      originAttributes,
    );
  }

  // User-entered URLs and bookmarks must not inherit system privileges. Null
  // and system principals are therefore rebound to a null principal while the
  // selected browser's origin attributes are retained.
  return securityManager.createNullPrincipal(originAttributes);
}

export function parseUserContextChoice(
  choice: string,
  workspaceUserContextId: number,
): { userContextId: number; explicit: boolean } | null {
  if (choice === "workspace") {
    return Number.isSafeInteger(workspaceUserContextId) &&
        workspaceUserContextId >= 0
      ? { userContextId: workspaceUserContextId, explicit: false }
      : null;
  }

  const userContextId = Number.parseInt(choice, 10);
  if (
    !/^\d+$/.test(choice) ||
    !Number.isSafeInteger(userContextId) ||
    userContextId < 0
  ) {
    return null;
  }

  return { userContextId, explicit: true };
}
