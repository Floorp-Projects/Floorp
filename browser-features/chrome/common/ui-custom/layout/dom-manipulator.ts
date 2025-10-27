/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect } from "solid-js";
import { config } from "#features-chrome/common/designs/configs.ts";

const DOM_LAYOUT_MANAGER_DEBUG_PREFIX = "[DOMLayoutManager]";

type ListenerRecord = {
  type: string;
  listener: EventListenerOrEventListenerObject;
  options?: boolean | AddEventListenerOptions;
  capture: boolean;
};

type ToolboxMirrorState = {
  installed: boolean;
  toolbox: XULElement | null;
  originalAdd?: EventTarget["addEventListener"];
  originalRemove?: EventTarget["removeEventListener"];
  listeners: ListenerRecord[];
  mirrorTarget: EventTarget | null;
  forwarderHandler: ((event: Event) => void) | null;
  forwarderTarget: EventTarget | null;
};

const toolboxMirrorState: ToolboxMirrorState = {
  installed: false,
  toolbox: null,
  listeners: [],
  mirrorTarget: null,
  forwarderHandler: null,
  forwarderTarget: null,
};

let installScheduled = false;

const FORWARDED_EVENT_TYPES = [
  "command",
  "click",
  "mousedown",
  "mouseup",
  "keypress",
  "keyup",
  "keydown",
  "contextmenu",
];

function normalizeCapture(
  options?: boolean | AddEventListenerOptions,
): boolean {
  if (typeof options === "boolean") {
    return options;
  }
  if (typeof options === "object" && options !== null) {
    return Boolean(options.capture);
  }
  return false;
}

function mirrorRecordToTarget(
  record: ListenerRecord,
  explicitTarget?: EventTarget | null,
): void {
  const target =
    explicitTarget ??
    (toolboxMirrorState.mirrorTarget ?? toolboxMirrorState.toolbox);
  const { toolbox } = toolboxMirrorState;
  if (!target || target === toolbox) {
    return;
  }

  try {
    target.addEventListener(
      record.type,
      record.listener,
      record.options as AddEventListenerOptions | boolean | undefined,
    );
  } catch (error) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to mirror listener for ${record.type}`,
      error,
    );
  }
}

function unmirrorRecordFromTarget(
  record: ListenerRecord,
  explicitTarget?: EventTarget | null,
): void {
  const target =
    explicitTarget ??
    (toolboxMirrorState.mirrorTarget ?? toolboxMirrorState.toolbox);
  const { toolbox } = toolboxMirrorState;
  if (!target || target === toolbox) {
    return;
  }

  try {
    target.removeEventListener(
      record.type,
      record.listener,
      record.options as EventListenerOptions | boolean | undefined,
    );
  } catch (error) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to remove mirrored listener for ${record.type}`,
      error,
    );
  }
}

function tryInstallNavigatorToolboxMirror(): boolean {
  if (toolboxMirrorState.installed) {
    return true;
  }

  const toolbox = document?.getElementById(
    "navigator-toolbox",
  ) as XULElement | null;
  if (!toolbox) {
    return false;
  }

  const originalAdd = toolbox.addEventListener;
  const originalRemove = toolbox.removeEventListener;
  const listeners: ListenerRecord[] = [];

  const patchedAdd: typeof toolbox.addEventListener = function patchedAdd(
    this: EventTarget,
    type,
    listener,
    options,
  ) {
    if (this === toolbox && listener) {
      const capture = normalizeCapture(options);
      const exists = listeners.some(
        (record) =>
          record.type === type &&
          record.listener === listener &&
          record.capture === capture,
      );
      if (!exists) {
        const record: ListenerRecord = {
          type,
          listener,
          options,
          capture,
        };
        listeners.push(record);
        mirrorRecordToTarget(record);
      }
    }

    return originalAdd.call(this, type, listener, options);
  };

  const patchedRemove: typeof toolbox.removeEventListener =
    function patchedRemove(this: EventTarget, type, listener, options) {
      if (this === toolbox && listener) {
        const capture = normalizeCapture(options);
        for (let index = listeners.length - 1; index >= 0; index -= 1) {
          const record = listeners[index];
          if (
            record.type === type &&
            record.listener === listener &&
            record.capture === capture
          ) {
            listeners.splice(index, 1);
            unmirrorRecordFromTarget(record);
            break;
          }
        }
      }

      return originalRemove.call(this, type, listener, options);
    };

  toolbox.addEventListener = patchedAdd;
  toolbox.removeEventListener = patchedRemove;

  toolboxMirrorState.installed = true;
  toolboxMirrorState.toolbox = toolbox;
  toolboxMirrorState.originalAdd = originalAdd;
  toolboxMirrorState.originalRemove = originalRemove;
  toolboxMirrorState.listeners = listeners;

  return true;
}

function scheduleInstallAttempt(): void {
  if (installScheduled || toolboxMirrorState.installed) {
    return;
  }
  installScheduled = true;

  const attempt = () => {
    installScheduled = false;
    if (!tryInstallNavigatorToolboxMirror()) {
      console.debug(
        `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} navigator-toolbox not available for mirroring`,
      );
    }
  };

  if (document?.readyState === "loading") {
    document.addEventListener(
      "DOMContentLoaded",
      () => {
        attempt();
      },
      { once: true },
    );
  } else {
    if (typeof globalThis.queueMicrotask === "function") {
      globalThis.queueMicrotask(() => attempt());
    } else {
      globalThis.setTimeout(() => attempt(), 0);
    }
  }
}

export function ensureNavigatorToolboxListenerMirroringInstalled(): void {
  if (toolboxMirrorState.installed) {
    return;
  }

  if (tryInstallNavigatorToolboxMirror()) {
    return;
  }

  scheduleInstallAttempt();
}

export function setNavigatorToolboxMirrorTarget(
  target: EventTarget | null,
): void {
  const previousTarget = toolboxMirrorState.mirrorTarget;
  if (previousTarget === target) {
    return;
  }

  if (previousTarget && previousTarget !== toolboxMirrorState.toolbox) {
    for (const record of toolboxMirrorState.listeners) {
      unmirrorRecordFromTarget(record, previousTarget);
    }
  }

  toolboxMirrorState.mirrorTarget = target;

  if (target && target !== toolboxMirrorState.toolbox) {
    for (const record of toolboxMirrorState.listeners) {
      mirrorRecordToTarget(record, target);
    }
  }

  updateForwardingTarget(target);
}

function updateForwardingTarget(target: EventTarget | null): void {
  if (toolboxMirrorState.forwarderTarget && toolboxMirrorState.forwarderHandler) {
    for (const type of FORWARDED_EVENT_TYPES) {
      toolboxMirrorState.forwarderTarget.removeEventListener(
        type,
        toolboxMirrorState.forwarderHandler,
        true,
      );
    }
  }

  toolboxMirrorState.forwarderTarget = null;
  toolboxMirrorState.forwarderHandler = null;

  if (!target || target === toolboxMirrorState.toolbox) {
    return;
  }
  const toolbox = toolboxMirrorState.toolbox;
  if (!toolbox) {
    return;
  }

  const handler = (event: Event) => {
    forwardEventToNavigatorToolbox(event, toolbox);
  };

  for (const type of FORWARDED_EVENT_TYPES) {
    target.addEventListener(type, handler, { capture: true });
  }

  toolboxMirrorState.forwarderTarget = target;
  toolboxMirrorState.forwarderHandler = handler;
}

function forwardEventToNavigatorToolbox(
  event: Event,
  toolbox: XULElement,
): void {
  if ((event as Record<string, unknown>).__domLayoutForwarded) {
    return;
  }

  const clone = cloneEventForForward(event);
  if (!clone) {
    return;
  }

  preserveEventState(event, clone);

  Object.defineProperty(clone, "__domLayoutForwarded", {
    value: true,
    enumerable: false,
    configurable: true,
  });

  try {
    toolbox.dispatchEvent(clone);
  } catch (error) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to forward ${clone.type} event to navigator-toolbox`,
      error,
    );
  }
}

function cloneEventForForward(event: Event): Event | null {
  const commonInit: EventInit = {
    bubbles: event.bubbles,
    cancelable: event.cancelable,
    composed: event.composed,
  };

  if (typeof DragEvent !== "undefined" && event instanceof DragEvent) {
    return new DragEvent(event.type, {
      ...commonInit,
      dataTransfer: event.dataTransfer ?? undefined,
      screenX: event.screenX,
      screenY: event.screenY,
      clientX: event.clientX,
      clientY: event.clientY,
      ctrlKey: event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey: event.altKey,
      metaKey: event.metaKey,
      button: event.button,
      buttons: event.buttons,
      relatedTarget: event.relatedTarget ?? undefined,
      view: event.view ?? undefined,
    });
  }

  if (event instanceof MouseEvent) {
    return new event.constructor(event.type, {
      ...commonInit,
      detail: event.detail,
      screenX: event.screenX,
      screenY: event.screenY,
      clientX: event.clientX,
      clientY: event.clientY,
      ctrlKey: event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey: event.altKey,
      metaKey: event.metaKey,
      button: event.button,
      buttons: event.buttons,
      relatedTarget: event.relatedTarget ?? undefined,
      movementX: event.movementX,
      movementY: event.movementY,
      view: event.view ?? undefined,
    });
  }

  if (event instanceof KeyboardEvent) {
    return new event.constructor(event.type, {
      ...commonInit,
      key: event.key,
      code: event.code,
      location: event.location,
      ctrlKey: event.ctrlKey,
      shiftKey: event.shiftKey,
      altKey: event.altKey,
      metaKey: event.metaKey,
      repeat: event.repeat,
      isComposing: event.isComposing,
      charCode: event.charCode,
      keyCode: event.keyCode,
    });
  }

  if (event instanceof CustomEvent) {
    return new CustomEvent(event.type, {
      ...commonInit,
      detail: event.detail,
    });
  }

  return new Event(event.type, commonInit);
}

function preserveEventState(original: Event, clone: Event): void {
  if ("button" in original && "button" in clone) {
    (clone as MouseEvent).initMouseEvent(
      clone.type,
      clone.bubbles,
      clone.cancelable,
      (original as MouseEvent).view,
      (original as MouseEvent).detail,
      (original as MouseEvent).screenX,
      (original as MouseEvent).screenY,
      (original as MouseEvent).clientX,
      (original as MouseEvent).clientY,
      (original as MouseEvent).ctrlKey,
      (original as MouseEvent).altKey,
      (original as MouseEvent).shiftKey,
      (original as MouseEvent).metaKey,
      (original as MouseEvent).button,
      (original as MouseEvent).relatedTarget ?? null,
    );
  }

  if ((original as Record<string, unknown>).defaultPrevented) {
    clone.preventDefault();
  }
}

export class DOMLayoutManager {
  private static readonly DEBUG_PREFIX = DOM_LAYOUT_MANAGER_DEBUG_PREFIX;

  // Store original navbar position for proper restoration
  private originalNavbarParent: Element | null = null;
  private originalNavbarNextSibling: Element | null = null;
  private isNavbarAtBottom = false;
  // Store original inline style attribute of floorp tabbar container so we can restore it
  private originalTabbarManageContainerStyle: string | null = null;
  private urlbarFixIntervalId: number | null = null;
  private urlbarMutationObserver: MutationObserver | null = null;
  private urlbarStableCounter = 0;
  private navbarBottomScheduleController: AbortController | null = null;
  private browserStartupPromise: Promise<void> | null = null;
  private browserStartupCompleted = false;
  private fallbackNavbarTarget: XULElement | null = null;
  private fallbackNavbarHandlers: Array<{
    type: string;
    listener: EventListener;
    options?: boolean | AddEventListenerOptions;
  }> = [];

  private get navBar(): XULElement {
    const element = document?.getElementById("nav-bar") as XULElement;
    return element;
  }

  private get fullscreenWrapper(): XULElement {
    const element = document?.getElementById("a11y-announcement") as XULElement;
    return element;
  }

  private get navigatorToolbox(): XULElement {
    const element = document?.getElementById("navigator-toolbox") as XULElement;
    return element;
  }

  private get personalToolbar(): XULElement {
    const element = document?.getElementById("PersonalToolbar") as XULElement;
    return element;
  }

  setupDOMEffects() {
    this.setupNavbarPosition();
  }

  /**
   * Reset the saved navbar position state
   * Useful when the DOM structure changes or for cleanup
   */
  private resetNavbarPositionState() {
    this.originalNavbarParent = null;
    this.originalNavbarNextSibling = null;
    this.isNavbarAtBottom = false;
    this.cancelNavbarBottomScheduling();
    setNavigatorToolboxMirrorTarget(null);
    this.detachFallbackNavbarHandlers();
  }

  private setupNavbarPosition() {
    createEffect(() => {
      const currentPosition = config().uiCustomization.navbar.position;
      try {
        if (currentPosition === "bottom") {
          this.moveNavbarToBottom();
        } else {
          this.moveNavbarToTop();
        }
      } catch (error: unknown) {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error in navbar position setup:`,
          error,
        );
      }
    });
  }

  private moveNavbarToBottom() {
    if (this.isNavbarAtBottom) {
      return;
    }
    if (this.browserStartupCompleted) {
      this.executeNavbarToBottom();
      return;
    }
    this.scheduleNavbarToBottomMove();
  }

  private scheduleNavbarToBottomMove() {
    if (this.navbarBottomScheduleController) {
      return;
    }

    const controller = new AbortController();
    this.navbarBottomScheduleController = controller;

    this.waitForBrowserStartup()
      .then(() => {
        if (controller.signal.aborted) {
          return;
        }

        this.navbarBottomScheduleController = null;
        this.executeNavbarToBottom();
      })
      .catch((error: unknown) => {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error waiting for browser startup:`,
          error,
        );

        if (controller.signal.aborted) {
          return;
        }

        this.navbarBottomScheduleController = null;
        this.executeNavbarToBottom();
      });
  }

  private cancelNavbarBottomScheduling() {
    if (this.navbarBottomScheduleController) {
      this.navbarBottomScheduleController.abort();
      this.navbarBottomScheduleController = null;
    }
  }

  private removeUrlbarPopoverAttribute() {
    const urlbar = document?.getElementById("urlbar");
    if (!urlbar) {
      return;
    }

    if (!urlbar.hasAttribute("popover")) {
      return;
    }

    try {
      urlbar.removeAttribute("popover");
    } catch (error) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Failed to remove popover attribute from urlbar`,
        error,
      );
    }
  }

  private executeNavbarToBottom() {
    if (this.isNavbarAtBottom) {
      return;
    }
    if (config().uiCustomization.navbar.position !== "bottom") {
      return;
    }

    const navbar = this.navBar;
    const fullscreenWrapper = this.fullscreenWrapper;
    const statusbar = document?.getElementById(
      "nora-statusbar",
    ) as XULElement | null;

    if (!navbar) {
      console.warn(`${DOMLayoutManager.DEBUG_PREFIX} navbar element not found`);
      return;
    }

    const insertionAnchor =
      statusbar && statusbar.isConnected ? statusbar : fullscreenWrapper;

    if (!insertionAnchor) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} No valid insertion anchor for navbar placement`,
      );
      return;
    }

    try {
      ensureNavigatorToolboxListenerMirroringInstalled();

      this.originalNavbarParent = navbar.parentElement;
      this.originalNavbarNextSibling = navbar.nextElementSibling;

      insertionAnchor.after(navbar);
      this.isNavbarAtBottom = true;
      setNavigatorToolboxMirrorTarget(navbar);
      this.attachFallbackNavbarHandlers(navbar);

      this.removeUrlbarPopoverAttribute();

      // Hide the floorp tabbar window manage container when navbar is at bottom
      try {
        const tabbarManage = document?.getElementById(
          "floorp-tabbar-window-manage-container",
        );
        if (tabbarManage) {
          // Save original inline style attribute so we can restore it later
          this.originalTabbarManageContainerStyle =
            tabbarManage.getAttribute("style");
          // Set display:none via attribute to avoid typing/library issues
          const newStyle = (
            this.originalTabbarManageContainerStyle ?? ""
          ).trim();
          const appended =
            newStyle.length > 0 ? `${newStyle};display:none;` : "display:none;";
          tabbarManage.setAttribute("style", appended);
        }
      } catch (error: unknown) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} Failed to hide floorp tabbar manage container`,
          error,
        );
      }

      // Delay urlbar fix to avoid early DOM mutation causing layout glitches (Issue #1936)
      this.scheduleFixUrlbarInputContainer();
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error in moveNavbarToBottom:`,
        error,
      );
    }
  }

  private moveNavbarToTop() {
    this.cancelNavbarBottomScheduling();
    // When changing from bottom to top position, a restart is required
    // to properly restore the original DOM structure
    if (this.isNavbarAtBottom) {
      console.info(
        `${DOMLayoutManager.DEBUG_PREFIX} Navbar position change requires restart to take effect`,
      );

      // Optionally show a notification to the user about restart requirement
      // This could be implemented with a toast notification or similar UI element

      return;
    }

    // If navbar is already at top, no action needed
  }

  /** Restore the floorp tabbar manage container inline style if it was changed. */
  private restoreTabbarManageContainer() {
    if (this.originalTabbarManageContainerStyle === null) {
      // No saved style, nothing to do
      return;
    }

    try {
      const tabbarManage = document?.getElementById(
        "floorp-tabbar-window-manage-container",
      );
      if (!tabbarManage) return;

      if (this.originalTabbarManageContainerStyle === null) {
        tabbarManage.removeAttribute("style");
      } else {
        tabbarManage.setAttribute(
          "style",
          this.originalTabbarManageContainerStyle,
        );
      }
    } catch (error: unknown) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} Failed to restore floorp tabbar manage container style`,
        error,
      );
    } finally {
      this.originalTabbarManageContainerStyle = null;
    }
  }

  private attachFallbackNavbarHandlers(navbar: XULElement): void {
    if (!navbar.isConnected) {
      return;
    }

    const toolbox = this.navigatorToolbox;
    if (toolbox && toolbox.contains(navbar)) {
      this.detachFallbackNavbarHandlers();
      return;
    }

    if (this.fallbackNavbarTarget === navbar && this.fallbackNavbarHandlers.length > 0) {
      return;
    }

    this.detachFallbackNavbarHandlers();

    const win = window as typeof window & {
      gSync?: {
        toggleAccountPanel?: (anchor: Element | null, event?: Event) => void;
      };
      gUnifiedExtensions?: {
        togglePanel?: (event?: Event) => void;
      };
    };

    const handleMouseDown: EventListener = (event: Event) => {
      if (!(event instanceof MouseEvent)) {
        return;
      }
      const element = (event.target as Element | null)?.closest(
        "#fxa-toolbar-menu-button, #unified-extensions-button",
      );
      if (!element) {
        return;
      }

      switch (element.id) {
        case "fxa-toolbar-menu-button":
          if (win.gSync?.toggleAccountPanel) {
            try {
              win.gSync.toggleAccountPanel(element, event);
            } catch (error) {
              console.warn(
                `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Fallback gSync.toggleAccountPanel failed`,
                error,
              );
            }
          }
          break;
        case "unified-extensions-button":
          if (win.gUnifiedExtensions?.togglePanel) {
            try {
              win.gUnifiedExtensions.togglePanel(event);
            } catch (error) {
              console.warn(
                `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Fallback gUnifiedExtensions.togglePanel failed`,
                error,
              );
            }
          }
          break;
        default:
          break;
      }
    };

    const handleKeyPress: EventListener = (event: Event) => {
      if (!(event instanceof KeyboardEvent)) {
        return;
      }
      const isLikeLeftClick =
        event.key === "Enter" || event.key === " " || event.key === "Spacebar";
      if (!isLikeLeftClick) {
        return;
      }

      const element = (event.target as Element | null)?.closest(
        "#fxa-toolbar-menu-button, #unified-extensions-button",
      );
      if (!element) {
        return;
      }

      switch (element.id) {
        case "fxa-toolbar-menu-button":
          if (win.gSync?.toggleAccountPanel) {
            try {
              win.gSync.toggleAccountPanel(element, event);
            } catch (error) {
              console.warn(
                `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Fallback gSync.toggleAccountPanel failed`,
                error,
              );
            }
          }
          break;
        case "unified-extensions-button":
          if (win.gUnifiedExtensions?.togglePanel) {
            try {
              win.gUnifiedExtensions.togglePanel(event);
            } catch (error) {
              console.warn(
                `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Fallback gUnifiedExtensions.togglePanel failed`,
                error,
              );
            }
          }
          break;
        default:
          break;
      }
    };

    const handlers: Array<{
      type: string;
      listener: EventListener;
      options?: boolean | AddEventListenerOptions;
    }> = [
      { type: "mousedown", listener: handleMouseDown, options: true },
      { type: "keypress", listener: handleKeyPress, options: true },
    ];

    for (const entry of handlers) {
      try {
        navbar.addEventListener(entry.type, entry.listener, entry.options);
      } catch (error) {
        console.warn(
          `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to attach fallback listener for ${entry.type}`,
          error,
        );
      }
    }

    this.fallbackNavbarTarget = navbar;
    this.fallbackNavbarHandlers = handlers;
  }

  private detachFallbackNavbarHandlers(): void {
    if (!this.fallbackNavbarTarget) {
      this.fallbackNavbarHandlers = [];
      return;
    }

    for (const entry of this.fallbackNavbarHandlers) {
      try {
        this.fallbackNavbarTarget.removeEventListener(
          entry.type,
          entry.listener,
          entry.options,
        );
      } catch (error) {
        console.warn(
          `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to detach fallback listener for ${entry.type}`,
          error,
        );
      }
    }

    this.fallbackNavbarHandlers = [];
    this.fallbackNavbarTarget = null;
  }

  /**
   * Schedule fix for urlbar input container with retries.
   * Sometimes Firefox builds urlbar sub-DOM asynchronously; executing too early causes
   * the address bar to render in the wrong place (Issue #1936).
   */
  private scheduleFixUrlbarInputContainer() {
    // Start after SessionStore initialization, then attempt several times with backoff.
    // deno-lint-ignore no-explicit-any
    const sessionStore = (globalThis as { SessionStore?: any }).SessionStore;
    if (!sessionStore?.promiseInitialized) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} SessionStore not ready for urlbar fix`,
      );
      return;
    }

    sessionStore.promiseInitialized
      .then(() => {
        this.retryFixUrlbarInputContainer();
        this.startUrlbarPositionPolling();
        this.startUrlbarMutationObserver();
      })
      .catch((error: unknown) => {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error waiting for SessionStore initialization:`,
          error,
        );
      });
  }

  private retryFixUrlbarInputContainer(attempt = 0) {
    const MAX_ATTEMPTS = 10;
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );

    if (urlbarView && urlbarInputContainer) {
      // Only move if not already placed
      if (urlbarView.nextElementSibling !== urlbarInputContainer) {
        try {
          urlbarView.after(urlbarInputContainer);
          console.info(
            `${DOMLayoutManager.DEBUG_PREFIX} urlbar input container positioned (attempt ${attempt})`,
          );
        } catch (error: unknown) {
          console.error(
            `${DOMLayoutManager.DEBUG_PREFIX} Error moving urlbar input container:`,
            error,
          );
        }
      }
      return; // Success or already positioned
    }

    if (attempt < MAX_ATTEMPTS) {
      const delay = 50 + attempt * 50; // incremental backoff
      setTimeout(() => this.retryFixUrlbarInputContainer(attempt + 1), delay);
    } else {
      if (!urlbarView) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} urlbarView element not found after retries`,
        );
      }
      if (!urlbarInputContainer) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} urlbarInputContainer element not found after retries`,
        );
      }
    }
  }

  /** Start lightweight polling for a short period to ensure late DOM tweaks don't revert layout. */
  private startUrlbarPositionPolling() {
    // Clear existing
    if (this.urlbarFixIntervalId) {
      clearInterval(this.urlbarFixIntervalId);
      this.urlbarFixIntervalId = null;
    }

    const MAX_DURATION_MS = 7000; // stop after 7s
    const REQUIRED_STABLE_TICKS = 5; // number of consecutive OK ticks before stopping
    const INTERVAL_MS = 300;
    const perf = globalThis.performance;
    const start = perf ? perf.now() : Date.now();
    this.urlbarStableCounter = 0;

    this.urlbarFixIntervalId = setInterval(() => {
      const ok = this.ensureUrlbarOrder();
      if (ok) {
        this.urlbarStableCounter++;
      } else {
        this.urlbarStableCounter = 0; // reset stability window
      }

      const elapsed = (perf ? perf.now() : Date.now()) - start;
      if (
        this.urlbarStableCounter >= REQUIRED_STABLE_TICKS ||
        elapsed > MAX_DURATION_MS
      ) {
        if (this.urlbarFixIntervalId) {
          clearInterval(this.urlbarFixIntervalId);
          this.urlbarFixIntervalId = null;
          console.info(
            `${DOMLayoutManager.DEBUG_PREFIX} Stopped urlbar polling (stable=${
              this.urlbarStableCounter >= REQUIRED_STABLE_TICKS
            }, elapsed=${Math.round(elapsed)}ms)`,
          );
        }
      }
    }, INTERVAL_MS) as unknown as number; // casting due to TS DOM lib / Gecko types mismatch
  }

  /** Use MutationObserver for structural changes after initial stabilization (customization UI, extensions, etc.) */
  private startUrlbarMutationObserver() {
    // Disconnect existing observer
    this.urlbarMutationObserver?.disconnect();

    const target = document?.getElementById("nav-bar");
    if (!target) return;

    this.urlbarMutationObserver = new MutationObserver((mutations) => {
      // Debounced check: if any childList changes might affect order
      const needsCheck = mutations.some((m) => m.type === "childList");
      if (needsCheck) {
        // microtask -> macrotask to let DOM settle
        setTimeout(() => this.ensureUrlbarOrder(), 0);
      }
    });

    try {
      this.urlbarMutationObserver.observe(target, {
        childList: true,
        subtree: true,
      });
    } catch (e) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} Failed to start MutationObserver`,
        e,
      );
    }
  }

  /** Ensure the urlbar input container is placed after .urlbarView. Returns true if layout is correct. */
  private ensureUrlbarOrder(): boolean {
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );
    if (!urlbarView || !urlbarInputContainer) return false;
    if (urlbarView.nextElementSibling === urlbarInputContainer) return true;
    try {
      urlbarView.after(urlbarInputContainer);
      console.info(
        `${DOMLayoutManager.DEBUG_PREFIX} ensureUrlbarOrder: corrected ordering`,
      );
      return true;
    } catch (e) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} ensureUrlbarOrder failed`,
        e,
      );
      return false;
    }
  }

  private restoreUrlbarInputContainer() {
    const urlbarView = document?.querySelector(".urlbarView");
    const urlbarInputContainer = document?.querySelector(
      ".urlbar-input-container",
    );

    if (!urlbarView) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} urlbarView element not found`,
      );
      return;
    }

    if (!urlbarInputContainer) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} urlbarInputContainer element not found`,
      );
      return;
    }

    try {
      urlbarView.before(urlbarInputContainer);
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error restoring urlbar input container:`,
        error,
      );
    }
  }

  private async waitForBrowserStartup(): Promise<void> {
    if (this.browserStartupCompleted) {
      return;
    }

    if (!this.browserStartupPromise) {
      this.browserStartupPromise = (async () => {
        try {
          await this.waitForDocumentLoad();

          const firefoxWindow = window as typeof window & {
            delayedStartupPromise?: Promise<unknown>;
            gBrowserInit?: {
              delayedStartupPromise?: Promise<unknown>;
              delayedStartupFinished?: boolean;
            };
          };

          const delayedPromise =
            firefoxWindow.delayedStartupPromise ??
            firefoxWindow.gBrowserInit?.delayedStartupPromise;

          if (delayedPromise && typeof delayedPromise.then === "function") {
            try {
              await delayedPromise;
              return;
            } catch (error) {
              console.warn(
                `${DOMLayoutManager.DEBUG_PREFIX} delayedStartupPromise rejected`,
                error,
              );
            }
          }

          await this.waitForDelayedStartup(firefoxWindow);
        } catch (error) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} waitForBrowserStartup failed`,
            error,
          );
        }
      })().finally(() => {
        this.browserStartupCompleted = true;
      });
    }

    await this.browserStartupPromise;
    this.removeUrlbarPopoverAttribute();
  }

  private waitForDocumentLoad(): Promise<void> {
    if (typeof window === "undefined" || document?.readyState === "complete") {
      return Promise.resolve();
    }

    return new Promise((resolve) => {
      globalThis.addEventListener("load", () => resolve(), { once: true });
    });
  }

  private async waitForDelayedStartup(
    firefoxWindow: typeof window & {
      gBrowserInit?: { delayedStartupFinished?: boolean };
    },
  ): Promise<void> {
    const OBSERVER_TOPIC = "browser-delayed-startup-finished";
    const INTERVAL_MS = 50;
    const MAX_WAIT_MS = 20000;

    if (firefoxWindow.gBrowserInit?.delayedStartupFinished) {
      return;
    }

    const servicesContainer = globalThis as typeof globalThis & {
      Services?: {
        obs?: {
          addObserver?: (observer: unknown, topic: string) => void;
          removeObserver?: (observer: unknown, topic: string) => void;
        };
      };
    };

    await new Promise<void>((resolve) => {
      const perf = globalThis.performance;
      const start = perf?.now ? perf.now() : Date.now();
      let settled = false;
      let intervalId: number | null = null;
      const services = servicesContainer.Services;

      type Observer = {
        observe: (subject: unknown, topic: string, data?: string) => void;
      };

      let observer: Observer | null = null;

      const finish = () => {
        if (settled) {
          return;
        }
        settled = true;

        if (intervalId !== null) {
          globalThis.clearInterval(intervalId as unknown as number);
          intervalId = null;
        }

        if (observer && services?.obs?.removeObserver) {
          try {
            services.obs.removeObserver(observer, OBSERVER_TOPIC);
          } catch (error) {
            console.warn(
              `${DOMLayoutManager.DEBUG_PREFIX} Failed to remove delayed-startup observer`,
              error,
            );
          }
        }

        resolve();
      };

      if (services?.obs?.addObserver && services.obs.removeObserver) {
        observer = {
          observe: (_subject, topic) => {
            if (topic === OBSERVER_TOPIC) {
              finish();
            }
          },
        };

        try {
          services.obs.addObserver(observer, OBSERVER_TOPIC);
        } catch (error) {
          console.warn(
            `${DOMLayoutManager.DEBUG_PREFIX} Failed to add delayed-startup observer`,
            error,
          );
          observer = null;
        }
      }

      const checkReady = () => {
        if (firefoxWindow.gBrowserInit?.delayedStartupFinished) {
          finish();
          return;
        }

        const now = perf?.now ? perf.now() : Date.now();
        if (now - start >= MAX_WAIT_MS) {
          finish();
        }
      };

      intervalId = globalThis.setInterval(
        checkReady,
        INTERVAL_MS,
      ) as unknown as number;
      checkReady();
    });
  }
}
