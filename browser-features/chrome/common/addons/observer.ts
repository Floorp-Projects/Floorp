/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  AddonPermissionTarget,
  AddonsContext,
  AddonsLogger,
  ChromeWebStoreInstallInfo,
  ChromeWebStoreWindow,
  CWSObserver,
  InstallInfo,
  InstallInfoState,
} from "./types.ts";
import type { NotificationCustomizer } from "./notification-customizer.ts";
export type { InstallInfoState } from "./types.ts";

const CWS_TOPIC = "floorp-chrome-web-store-install-started";
const PERMISSION_TOPIC = "webextension-permission-prompt";

function getWindowContext(win: ChromeWebStoreWindow): AddonsContext {
  return win.__floorpAddonsContext ??= {
    state: { pendingChromeWebStoreInstall: null },
    customizer: null,
    logger: null,
    observer: null,
    savedDescriptionChildren: null,
    animationFrames: new Set(),
    disposed: false,
  };
}

function updateWindowContext(
  win: ChromeWebStoreWindow,
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: AddonsLogger,
): AddonsContext {
  const ctx = getWindowContext(win);
  if (ctx.state.pendingChromeWebStoreInstall) {
    state.pendingChromeWebStoreInstall = ctx.state.pendingChromeWebStoreInstall;
  }
  ctx.state = state;
  ctx.customizer = customizer;
  ctx.logger = logger;
  return ctx;
}

function isActive(win: ChromeWebStoreWindow, ctx: AddonsContext): boolean {
  return !ctx.disposed && !win.closed && win.__floorpAddonsContext === ctx;
}

function cancelFrames(win: ChromeWebStoreWindow, ctx: AddonsContext): void {
  for (const id of ctx.animationFrames) win.cancelAnimationFrame(id);
  ctx.animationFrames.clear();
}

function afterFrames(
  win: ChromeWebStoreWindow,
  ctx: AddonsContext,
  callback: () => void,
  remaining = 2,
): void {
  if (!isActive(win, ctx)) return;
  const id = win.requestAnimationFrame(() => {
    ctx.animationFrames.delete(id);
    if (!isActive(win, ctx)) return;
    if (remaining > 1) afterFrames(win, ctx, callback, remaining - 1);
    else callback();
  });
  ctx.animationFrames.add(id);
}

/** Resolve the same owner window as ExtensionsUI.getTabBrowser(target). */
function getPermissionWindow(target: unknown): ChromeWebStoreWindow | null {
  if (!target || typeof target !== "object") return null;
  let browser = target as AddonPermissionTarget;
  const visited = new Set<AddonPermissionTarget>();
  while (
    browser.documentGlobal?.docShell?.itemType !== Ci.nsIDocShell.typeChrome
  ) {
    if (visited.has(browser)) return null;
    visited.add(browser);
    const parent = browser.documentGlobal?.docShell?.chromeEventHandler;
    if (!parent) return null;
    browser = parent;
  }
  const owner = browser.documentGlobal;
  if (browser.getAttribute?.("webextension-view-type") === "sidebar") {
    return owner?.browsingContext?.topChromeWindow ?? null;
  }
  return owner as ChromeWebStoreWindow;
}

export function getChromeWebStoreInstallInfo(
  win: ChromeWebStoreWindow = window,
): ChromeWebStoreInstallInfo | null {
  return win.__floorpAddonsContext?.state.pendingChromeWebStoreInstall ??
    win.__chromeWebStoreInstallInfo ?? null;
}

export function clearChromeWebStoreInstallInfo(
  win: ChromeWebStoreWindow = window,
): void {
  const ctx = win.__floorpAddonsContext;
  if (ctx) ctx.state.pendingChromeWebStoreInstall = null;
  delete win.__chromeWebStoreInstallInfo;
}

export function saveOriginalDescriptionChildren(
  win: ChromeWebStoreWindow = window,
): void {
  const ctx = getWindowContext(win);
  if (ctx.savedDescriptionChildren !== null) return;
  const container = win.document.getElementById(
    "addon-webext-permissions-notification",
  )?.querySelector(".popup-notification-description");
  if (container) {
    ctx.savedDescriptionChildren = Array.from(container.childNodes).map(
      (node) => node.cloneNode(true),
    );
  }
}

function cleanupCWSCustomization(
  win: ChromeWebStoreWindow,
  ctx: AddonsContext,
): void {
  const notification = win.document.getElementById(
    "addon-webext-permissions-notification",
  );
  if (!notification) return;
  const container = notification.querySelector(
    ".popup-notification-description",
  );
  if (container?.querySelector(".chrome-web-store-message")) {
    container.textContent = "";
    for (const child of ctx.savedDescriptionChildren ?? []) {
      container.appendChild(child.cloneNode(true));
    }
  }
  ctx.savedDescriptionChildren = null;
  notification.querySelector(".chrome-extension-warning")?.remove();
  const intro = notification.querySelector("#addon-webext-perm-intro") as
    | HTMLElement
    | null;
  intro?.style.removeProperty("display");
  notification.removeAttribute("data-cws-customized");
}

export function createCWSObserver(
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: AddonsLogger,
  win: ChromeWebStoreWindow = window,
): CWSObserver {
  if (win.closed) {
    throw new Error("Cannot observe addon installs for a closed window");
  }
  const ctx = updateWindowContext(win, state, customizer, logger);
  if (ctx.observer) return ctx.observer;
  const observer: CWSObserver = {
    observe(subject, topic) {
      if (!isActive(win, ctx)) return;
      if (topic === CWS_TOPIC) {
        const info =
          (subject as { wrappedJSObject?: ChromeWebStoreInstallInfo } | null)
            ?.wrappedJSObject;
        // The actor sets the target-window marker before its shared notification.
        const local = win.__chromeWebStoreInstallInfo;
        if (info && local && local.id === info.id && local.name === info.name) {
          ctx.state.pendingChromeWebStoreInstall = {
            name: info.name,
            id: info.id,
          };
        }
      } else if (topic === PERMISSION_TOPIC) {
        const target =
          (subject as { wrappedJSObject?: { target?: unknown } } | null)
            ?.wrappedJSObject?.target;
        if (getPermissionWindow(target) !== win) return;
        const info = getChromeWebStoreInstallInfo(win);
        afterFrames(win, ctx, () => {
          if (info) {
            saveOriginalDescriptionChildren(win);
            ctx.customizer?.customizeWebExtPermissionPrompt(info);
          } else cleanupCWSCustomization(win, ctx);
          clearChromeWebStoreInstallInfo(win);
        });
      }
    },
    dispose() {
      if (ctx.disposed) return;
      ctx.disposed = true;
      cancelFrames(win, ctx);
      for (const topic of [CWS_TOPIC, PERMISSION_TOPIC]) {
        try {
          Services.obs.removeObserver(observer, topic);
        } catch { /* already removed */ }
      }
      ctx.observer = null;
      ctx.customizer = null;
      ctx.logger = null;
      ctx.savedDescriptionChildren = null;
      ctx.state.pendingChromeWebStoreInstall = null;
      if (win.__floorpAddonsContext === ctx) {
        delete win.__chromeWebStoreInstallInfo;
        delete win.__floorpAddonsContext;
      }
    },
  };
  ctx.observer = observer;
  try {
    Services.obs.addObserver(observer, CWS_TOPIC);
    Services.obs.addObserver(observer, PERMISSION_TOPIC);
  } catch (error) {
    observer.dispose?.();
    throw error;
  }
  return observer;
}

export function removeCWSObserver(observer: CWSObserver | null): void {
  if (!observer) return;
  if (observer.dispose) {
    observer.dispose();
    return;
  }
  for (const topic of [CWS_TOPIC, PERMISSION_TOPIC]) {
    try {
      Services.obs.removeObserver(observer, topic);
    } catch { /* not registered */ }
  }
}

export function overrideInstallConfirmation(
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: AddonsLogger,
  win: ChromeWebStoreWindow = window,
): (() => void) | null {
  const ctx = updateWindowContext(win, state, customizer, logger);
  const native = win.gXPInstallObserver;
  if (!native) {
    logger.warn?.("gXPInstallObserver not found");
    return null;
  }
  const original = native.showInstallConfirmation;
  let active = true;
  const showInstallConfirmation = (
    browser: unknown,
    info: InstallInfo,
    height?: number,
  ) => {
    original.call(native, browser, info, height);
    if (
      !active || !isActive(win, ctx) || getPermissionWindow(browser) !== win
    ) return;
    const cwsInfo = getChromeWebStoreInstallInfo(win);
    if (!cwsInfo) return;
    afterFrames(win, ctx, () => {
      if (
        !active || native.showInstallConfirmation !== showInstallConfirmation
      ) return;
      ctx.customizer?.customizeNotificationContent(cwsInfo);
      clearChromeWebStoreInstallInfo(win);
    }, 1);
  };
  native.showInstallConfirmation = showInstallConfirmation;
  return () => {
    active = false;
    cancelFrames(win, ctx);
    if (native.showInstallConfirmation === showInstallConfirmation) {
      native.showInstallConfirmation = original;
      clearChromeWebStoreInstallInfo(win);
    }
  };
}
