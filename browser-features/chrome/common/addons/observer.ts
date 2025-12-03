/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  ChromeWebStoreInstallInfo,
  ChromeWebStoreWindow,
  CWSObserver,
  InstallInfo,
  XPInstallObserver,
} from "./types";
import { NotificationCustomizer } from "./notification-customizer";

export interface InstallInfoState {
  pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
}

/**
 * Global context for the addons module
 */
interface AddonsContext {
  state: InstallInfoState;
  customizer: NotificationCustomizer | null;
  logger: { debug: (...args: unknown[]) => void } | null;
  observer: CWSObserver | null;
  /** Saved original description children for restoration */
  savedDescriptionChildren: Node[] | null;
}

/**
 * Global object extension interface for storing context
 * We use the global object from Services to ensure it's shared across all windows
 */
interface AddonsGlobal {
  __floorpAddonsContext?: AddonsContext;
}

/**
 * Get the truly global scope (shared across all windows)
 */
function getGlobalScope(): AddonsGlobal {
  // Use Cu.getGlobalForObject to get the global scope from Services
  // This is shared across all chrome windows
  return Cu.getGlobalForObject(Services) as AddonsGlobal;
}

/**
 * Get or create the global context
 * This ensures all references persist across HMR reloads and across windows
 */
function getGlobalContext(): AddonsContext {
  const global = getGlobalScope();
  if (!global.__floorpAddonsContext) {
    global.__floorpAddonsContext = {
      state: { pendingChromeWebStoreInstall: null },
      customizer: null,
      logger: null,
      observer: null,
      savedDescriptionChildren: null,
    };
  }
  return global.__floorpAddonsContext;
}

/**
 * Update the global context with new references
 */
function updateGlobalContext(
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: { debug: (...args: unknown[]) => void },
): AddonsContext {
  const ctx = getGlobalContext();
  // Merge state (preserve pending install info if exists)
  if (ctx.state?.pendingChromeWebStoreInstall) {
    state.pendingChromeWebStoreInstall = ctx.state.pendingChromeWebStoreInstall;
  }
  ctx.state = state;
  ctx.customizer = customizer;
  ctx.logger = logger;
  return ctx;
}

/**
 * Get the current Chrome Web Store install info
 */
export function getChromeWebStoreInstallInfo(): ChromeWebStoreInstallInfo | null {
  const ctx = getGlobalContext();
  // Try global context state first
  if (ctx.state?.pendingChromeWebStoreInstall) {
    return ctx.state.pendingChromeWebStoreInstall;
  }
  // Finally check window install info
  const win = window as ChromeWebStoreWindow;
  return win.__chromeWebStoreInstallInfo ?? null;
}

/**
 * Clear the Chrome Web Store install info
 */
export function clearChromeWebStoreInstallInfo(): void {
  const ctx = getGlobalContext();
  // Clear context state
  if (ctx.state) {
    ctx.state.pendingChromeWebStoreInstall = null;
  }
  // Also clear window install info from all windows
  const win = window as ChromeWebStoreWindow;
  delete win.__chromeWebStoreInstallInfo;
  
  // Also try to clear from the global scope
  const global = getGlobalScope() as ChromeWebStoreWindow;
  delete global.__chromeWebStoreInstallInfo;
}

/**
 * Save the original description children before CWS customization
 * This should be called before modifying the description container
 */
export function saveOriginalDescriptionChildren(): void {
  const ctx = getGlobalContext();
  if (ctx.savedDescriptionChildren !== null) {
    // Already saved
    return;
  }

  const notification = document.getElementById(
    "addon-webext-permissions-notification",
  );
  if (!notification) {
    return;
  }

  const descriptionContainer = notification.querySelector(
    ".popup-notification-description",
  );
  if (descriptionContainer) {
    ctx.savedDescriptionChildren = Array.from(descriptionContainer.childNodes).map(
      (node) => node.cloneNode(true),
    );
  }
}

/**
 * Clean up CWS customization from the DOM directly
 * This is used when a normal addon is being installed to ensure
 * no Chrome extension UI remnants are shown.
 * Uses saved original nodes for restoration instead of reconstructing DOM.
 */
function cleanupCWSCustomization(): void {
  const ctx = getGlobalContext();
  const notification = document.getElementById(
    "addon-webext-permissions-notification",
  );
  if (!notification) {
    return;
  }

  // Check if we need to restore the description
  const descriptionContainer = notification.querySelector(
    ".popup-notification-description",
  );
  const cwsMessage = descriptionContainer?.querySelector(".chrome-web-store-message");
  
  if (cwsMessage && descriptionContainer) {
    // Remove all current children
    descriptionContainer.textContent = "";
    
    // Restore saved original children if available
    if (ctx.savedDescriptionChildren !== null) {
      for (const child of ctx.savedDescriptionChildren) {
        descriptionContainer.appendChild(child.cloneNode(true));
      }
      ctx.savedDescriptionChildren = null;
    }
  }

  // Remove the compatibility warning if present
  const warning = notification.querySelector(".chrome-extension-warning");
  if (warning) {
    warning.remove();
  }

  // Restore the intro element visibility
  const introElement = notification.querySelector(
    "#addon-webext-perm-intro",
  ) as HTMLElement | null;
  if (introElement) {
    introElement.style.removeProperty("display");
  }

  // Remove the customized attribute
  notification.removeAttribute("data-cws-customized");
}

/**
 * Create and register the Chrome Web Store install observer
 */
export function createCWSObserver(
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: { debug: (...args: unknown[]) => void },
): CWSObserver {
  // First, update the global context with new references
  // This ensures customizer is available before any observer callback fires
  const ctx = updateGlobalContext(state, customizer, logger);

  // If observer already exists, just update the context and return
  // The observer callbacks will use getGlobalContext() to get fresh references
  if (ctx.observer) {
    logger.debug(
      "Observer already registered, context updated with new customizer",
    );
    return ctx.observer;
  }

  const observer: CWSObserver = {
    observe: (subject: unknown, topic: string) => {
      // Always get fresh references from global context
      const currentCtx = getGlobalContext();

      if (topic === "floorp-chrome-web-store-install-started") {
        const wrapped = subject as {
          wrappedJSObject?: ChromeWebStoreInstallInfo;
        };
        if (wrapped.wrappedJSObject && currentCtx.state) {
          currentCtx.state.pendingChromeWebStoreInstall =
            wrapped.wrappedJSObject;
          currentCtx.logger?.debug(
            "Received install notification:",
            currentCtx.state.pendingChromeWebStoreInstall,
          );
        }
      } else if (topic === "webextension-permission-prompt") {
        const cwsInfo = getChromeWebStoreInstallInfo();

        if (cwsInfo) {
          currentCtx.logger?.debug(
            "webextension-permission-prompt received for CWS install",
          );

          // Create a new customizer for this window context if needed
          const getOrCreateCustomizer = (): NotificationCustomizer => {
            const ctx = getGlobalContext();
            if (!ctx.customizer) {
              ctx.customizer = new NotificationCustomizer();
            }
            return ctx.customizer;
          };

          requestAnimationFrame(() => {
            requestAnimationFrame(() => {
              // Save original children before customization
              saveOriginalDescriptionChildren();
              const customizer = getOrCreateCustomizer();
              customizer.customizeWebExtPermissionPrompt(cwsInfo);
              clearChromeWebStoreInstallInfo();
            });
          });
        } else {
          currentCtx.logger?.debug(
            "webextension-permission-prompt received for normal addon",
          );

          requestAnimationFrame(() => {
            requestAnimationFrame(() => {
              // Clean up CWS-specific elements from DOM directly
              // Firefox has already populated the dialog with the correct addon info
              cleanupCWSCustomization();
              
              // Also ensure install info is cleared
              clearChromeWebStoreInstallInfo();
            });
          });
        }
      }
    },
  };

  // Store observer in context
  ctx.observer = observer;

  Services.obs.addObserver(observer, "floorp-chrome-web-store-install-started");
  Services.obs.addObserver(observer, "webextension-permission-prompt");
  logger.debug(
    "Observer registered for floorp-chrome-web-store-install-started",
  );
  logger.debug("Observer registered for webextension-permission-prompt");

  return observer;
}

/**
 * Remove the Chrome Web Store install observer
 */
export function removeCWSObserver(observer: CWSObserver | null): void {
  if (!observer) {
    return;
  }

  try {
    Services.obs.removeObserver(
      observer,
      "floorp-chrome-web-store-install-started",
    );
    Services.obs.removeObserver(observer, "webextension-permission-prompt");
  } catch {
    // Observer might not be registered
  }
}

/**
 * Override the install confirmation dialog for Chrome extensions
 */
export function overrideInstallConfirmation(
  state: InstallInfoState,
  customizer: NotificationCustomizer,
  logger: {
    warn: (...args: unknown[]) => void;
    debug: (...args: unknown[]) => void;
    info: (...args: unknown[]) => void;
  },
): (() => void) | null {
  // Update global context with new references
  updateGlobalContext(
    state,
    customizer,
    logger as { debug: (...args: unknown[]) => void },
  );

  const gXPInstallObserver = (
    window as Window & { gXPInstallObserver?: XPInstallObserver }
  ).gXPInstallObserver;

  if (!gXPInstallObserver) {
    logger.warn("gXPInstallObserver not found");
    return null;
  }

  const originalShowInstallConfirmation =
    gXPInstallObserver.showInstallConfirmation.bind(gXPInstallObserver);

  gXPInstallObserver.showInstallConfirmation = (
    browser: unknown,
    installInfo: InstallInfo,
    height?: number,
  ): void => {
    // Always get fresh references from global context
    const ctx = getGlobalContext();
    const cwsInfo = getChromeWebStoreInstallInfo();
    ctx.logger?.debug("showInstallConfirmation called, cwsInfo:", cwsInfo);

    originalShowInstallConfirmation(browser, installInfo, height);

    if (!cwsInfo) {
      ctx.logger?.debug(
        "Not a Chrome Web Store install, skipping customization",
      );
      return;
    }

    ctx.logger?.debug(
      "Scheduling customization for Chrome extension:",
      cwsInfo.name,
    );
    requestAnimationFrame(() => {
      const freshCtx = getGlobalContext();
      freshCtx.logger?.debug("Customizing notification content...");
      freshCtx.customizer?.customizeNotificationContent(cwsInfo);
      clearChromeWebStoreInstallInfo();
    });
  };

  logger.info("Install confirmation customization initialized");

  // Return cleanup function
  return () => {
    gXPInstallObserver.showInstallConfirmation =
      originalShowInstallConfirmation;
    clearChromeWebStoreInstallInfo();
    logger.info("Install confirmation customization cleaned up");
  };
}
