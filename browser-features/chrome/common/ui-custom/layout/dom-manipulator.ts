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
    toolboxMirrorState.mirrorTarget ??
    toolboxMirrorState.toolbox;
  const { toolbox } = toolboxMirrorState;
  if (!target || target === toolbox) {
    return;
  }

  try {
    target.addEventListener(
      record.type,
      record.listener as EventListener,
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
    toolboxMirrorState.mirrorTarget ??
    toolboxMirrorState.toolbox;
  const { toolbox } = toolboxMirrorState;
  if (!target || target === toolbox) {
    return;
  }

  try {
    target.removeEventListener(
      record.type,
      record.listener as EventListener,
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
    type: string,
    listener: EventListenerOrEventListenerObject | null,
    options?: boolean | AddEventListenerOptions,
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

    return originalAdd.call(
      this,
      type,
      listener as EventListenerOrEventListenerObject,
      options,
    );
  };

  const patchedRemove: typeof toolbox.removeEventListener =
    function patchedRemove(
      this: EventTarget,
      type: string,
      listener: EventListenerOrEventListenerObject | null,
      options?: boolean | EventListenerOptions,
    ) {
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

      return originalRemove.call(
        this,
        type,
        listener as EventListenerOrEventListenerObject,
        options,
      );
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
    tryInstallNavigatorToolboxMirror();
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
  if (
    toolboxMirrorState.forwarderTarget &&
    toolboxMirrorState.forwarderHandler
  ) {
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
    const target = event.target as Element | null;

    // Special handling for star-button (bookmark button) - check before forwarding
    if (event.type === "click") {
      const starButton = isStarButton(target);
      if (starButton && handleStarButtonClick(event, starButton)) {
        // Event was handled, don't forward
        return;
      }
    }

    forwardEventToNavigatorToolbox(event, toolbox);
  };

  for (const type of FORWARDED_EVENT_TYPES) {
    target.addEventListener(type, handler, { capture: true });
  }

  toolboxMirrorState.forwarderTarget = target;
  toolboxMirrorState.forwarderHandler = handler;
}

/**
 * Checks if an element is a XUL button that should generate command events
 */
function isXULButton(element: Element | null): boolean {
  if (!element) {
    return false;
  }

  // Check if element is inside a XUL button or has button-like attributes
  const button = element.closest("toolbarbutton, button");
  if (!button) {
    return false;
  }

  // Check if it's a URL bar button (star-button, reload-button, etc.)
  const id = button.id || "";
  const isUrlbarButton =
    id.includes("button") ||
    id.includes("star") ||
    id.includes("reload") ||
    id.includes("stop") ||
    button.classList.contains("urlbar-button") ||
    button.classList.contains("urlbar-page-action");

  return isUrlbarButton;
}

/**
 * Checks if an element is the star-button (bookmark button)
 */
function isStarButton(element: Element | null): XULElement | null {
  if (!element) {
    return null;
  }

  const targetId = element.id || "";
  const isStarButtonById =
    targetId === "star-button" || targetId === "star-button-box";
  const starButtonElement = element.closest(
    "#star-button, #star-button-box",
  ) as XULElement | null;

  if (isStarButtonById || starButtonElement) {
    return (starButtonElement || element) as XULElement | null;
  }

  return null;
}

/**
 * Handles star-button (bookmark button) clicks by calling PlacesCommandHook.bookmarkPage()
 * @param event The click or command event
 * @param buttonElement The star-button element
 * @returns true if the event was handled, false otherwise
 */
function handleStarButtonClick(
  event: Event,
  _buttonElement: XULElement,
): boolean {
  try {
    const win = window as typeof window & {
      PlacesCommandHook?: {
        bookmarkPage: () => void;
      };
    };
    if (win.PlacesCommandHook?.bookmarkPage) {
      win.PlacesCommandHook.bookmarkPage();
      // Prevent default behavior and stop propagation since we handled it
      event.preventDefault();
      event.stopPropagation();
      event.stopImmediatePropagation();
      return true;
    } else {
      console.warn(
        `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} PlacesCommandHook.bookmarkPage is not available`,
      );
    }
  } catch (err) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to call bookmarkPage() for star-button`,
      err,
    );
  }
  return false;
}

function forwardEventToNavigatorToolbox(
  event: Event,
  toolbox: XULElement,
): void {
  if ((event as unknown as Record<string, unknown>).__domLayoutForwarded) {
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
    // Command events from bookmark buttons are handled in attachBookmarkBarEventForwarder
    // Skip processing here to avoid duplicate handling
    if (clone.type === "command") {
      const originalTarget = event.target as Element | null;
      if (
        originalTarget &&
        originalTarget.isConnected &&
        originalTarget.classList.contains("bookmark-item")
      ) {
        // Already handled in attachBookmarkBarEventForwarder, skip here
        return;
      }

      // Special handling for star-button (bookmark button)
      // Call bookmarkPage() directly instead of relying on event forwarding
      const starButton = isStarButton(originalTarget);
      if (starButton && handleStarButtonClick(event, starButton)) {
        // Event was handled, don't forward
        return;
      }
    }

    // For click events on XUL buttons, also generate and forward a command event
    // This is necessary because XUL buttons generate command events from clicks,
    // but this doesn't happen automatically when the navbar is outside navigator-toolbox
    if (clone.type === "click") {
      const originalTarget = event.target as Element | null;

      // Special handling for star-button (bookmark button)
      // Check for star-button first, before isXULButton check
      const starButton = isStarButton(originalTarget);
      if (starButton && handleStarButtonClick(event, starButton)) {
        // Event was handled, don't forward
        return;
      }

      if (originalTarget && isXULButton(originalTarget)) {
        const button = originalTarget.closest(
          "toolbarbutton, button",
        ) as XULElement | null;
        if (button && button.id) {
          // Try to call doCommand() first, which is the standard way to trigger XUL button actions
          let doCommandSucceeded = false;
          try {
            if (
              typeof (button as XULElement & { doCommand?: () => void })
                .doCommand === "function"
            ) {
              (button as XULElement & { doCommand: () => void }).doCommand();
              doCommandSucceeded = true;
            }
          } catch {
            // doCommand() failed, fall back to command event
          }

          // If doCommand() didn't work, generate a command event from the button element itself
          // This ensures the event originates from the correct element
          if (!doCommandSucceeded) {
            try {
              // Dispatch command event directly from the button element
              // This is more reliable than forwarding from navigator-toolbox
              const commandEvent = new CustomEvent("command", {
                bubbles: true,
                cancelable: true,
                composed: true,
              });

              // Copy event properties from the original click event
              if (event instanceof MouseEvent) {
                Object.defineProperty(commandEvent, "ctrlKey", {
                  value: event.ctrlKey,
                  enumerable: true,
                  configurable: true,
                });
                Object.defineProperty(commandEvent, "shiftKey", {
                  value: event.shiftKey,
                  enumerable: true,
                  configurable: true,
                });
                Object.defineProperty(commandEvent, "altKey", {
                  value: event.altKey,
                  enumerable: true,
                  configurable: true,
                });
                Object.defineProperty(commandEvent, "metaKey", {
                  value: event.metaKey,
                  enumerable: true,
                  configurable: true,
                });
              }

              // Dispatch the command event from the button element itself
              // This ensures event listeners attached to the button or its ancestors can catch it
              button.dispatchEvent(commandEvent);

              // Also forward to navigator-toolbox in case listeners are attached there
              forwardEventToNavigatorToolbox(commandEvent, toolbox);
            } catch (err) {
              console.warn(
                `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to generate command event for XUL button`,
                err,
              );
            }
          }
        }
      }
    }

    // For other events or if original target dispatch failed, dispatch from toolbox
    // Make sure the event bubbles so listeners can catch it
    if (!clone.bubbles) {
      Object.defineProperty(clone, "bubbles", {
        value: true,
        writable: false,
        configurable: true,
      });
    }

    toolbox.dispatchEvent(clone);
  } catch (error) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to forward ${clone.type} event to navigator-toolbox`,
      error,
    );
  }
}

function cloneEventForForward(event: Event): Event | null {
  // Ensure events bubble so they can be caught by listeners
  const commonInit: EventInit = {
    bubbles: true, // Force bubbling for forwarded events
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
    return new MouseEvent(event.type, {
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
    return new KeyboardEvent(event.type, {
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
    // For command events, preserve the original event structure
    const customEvent = new CustomEvent(event.type, {
      ...commonInit,
      detail: event.detail,
    });
    // Preserve target for command events so navigator-toolbox listeners can identify the source
    Object.defineProperty(customEvent, "originalTarget", {
      value: event.target,
      enumerable: false,
      configurable: true,
    });
    return customEvent;
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

  if ((original as unknown as Record<string, unknown>).defaultPrevented) {
    clone.preventDefault();
  }
}

/**
 * Loads a bookmark URI using gBrowser.loadURI or gBrowser.addTab
 * @param uri The bookmark URI to load
 * @param mouseEvent The mouse event to determine modifier keys
 * @returns true if the URI was loaded successfully, false otherwise
 */
function loadBookmarkURI(uri: string, mouseEvent: MouseEvent): boolean {
  const win = window as typeof window & {
    gBrowser?: {
      loadURI: (uri: unknown, options?: unknown) => void;
      addTab: (
        uri: string,
        options?: {
          triggeringPrincipal?: unknown;
          inBackground?: boolean;
        },
      ) => unknown;
    };
  };
  const services = globalThis as typeof globalThis & {
    Services?: {
      scriptSecurityManager?: {
        getSystemPrincipal: () => unknown;
      };
      io?: {
        newURI: (uri: string) => unknown;
      };
    };
  };

  if (!win.gBrowser || !services.Services) {
    return false;
  }

  try {
    // Determine how to open the bookmark based on modifier keys
    // Ctrl/Cmd/Shift opens in new tab
    const openInNewTab =
      mouseEvent.ctrlKey || mouseEvent.metaKey || mouseEvent.shiftKey;

    const principal =
      services.Services.scriptSecurityManager?.getSystemPrincipal();
    const uriObject = services.Services.io?.newURI(uri);

    if (openInNewTab && win.gBrowser.addTab) {
      // Open in new tab
      win.gBrowser.addTab(uri, {
        triggeringPrincipal: principal,
        inBackground: false,
      });
    } else if (win.gBrowser.loadURI && uriObject) {
      // Open in current tab - need to convert string URI to URI object
      win.gBrowser.loadURI(uriObject, {
        triggeringPrincipal: principal,
      });
    }

    return true;
  } catch (err) {
    console.warn(
      `${DOM_LAYOUT_MANAGER_DEBUG_PREFIX} Failed to load bookmark URI`,
      err,
    );
    return false;
  }
}

/**
 * Handles command events from bookmark buttons
 * @param event The command event
 * @returns true if the event was handled, false otherwise
 */
function handleBookmarkCommandEvent(event: Event): boolean {
  const target = event.target as Element | null;
  if (!target || !target.classList.contains("bookmark-item")) {
    return false;
  }

  // Get bookmark URI from the button's _placesNode property
  const bookmarkButton = target as XULElement & {
    _placesNode?: { uri?: string };
  };
  const uri = bookmarkButton._placesNode?.uri;
  if (!uri) {
    return false;
  }

  const mouseEvent = event as MouseEvent;
  const success = loadBookmarkURI(uri, mouseEvent);

  if (success) {
    // Prevent default behavior and stop propagation since we handled it
    event.preventDefault();
    event.stopPropagation();
    event.stopImmediatePropagation();
  }

  return success;
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

  private get appContent(): XULElement {
    const element = document?.getElementById("appcontent") as XULElement;
    return element;
  }

  // Store original bookmark bar position for proper restoration
  private originalBookmarkBarParent: Element | null = null;
  private originalBookmarkBarNextSibling: Element | null = null;
  private isBookmarkBarAtBottom = false;
  private bookmarkBarForwarderHandler: ((event: Event) => void) | null = null;

  setupDOMEffects() {
    this.setupNavbarPosition();
    this.setupBookmarkBarPosition();
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

      // If bookmark bar is also at bottom, ensure it's positioned after navbar
      // and attach event forwarder so bookmark bar buttons work
      if (this.isBookmarkBarAtBottom) {
        const bookmarkBar = this.personalToolbar;
        if (bookmarkBar && bookmarkBar.isConnected) {
          // Move bookmark bar after navbar if it's not already there
          if (bookmarkBar.previousElementSibling !== navbar) {
            navbar.after(bookmarkBar);
          }
          // Ensure bookmark bar event forwarder is attached
          this.attachBookmarkBarEventForwarder(bookmarkBar);
        }
      }

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

    if (
      this.fallbackNavbarTarget === navbar &&
      this.fallbackNavbarHandlers.length > 0
    ) {
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

  private setupBookmarkBarPosition() {
    createEffect(() => {
      const currentPosition =
        config().uiCustomization.bookmarkBar?.position ?? "top";
      // Track navbar position to ensure bookmark bar is placed correctly
      // This creates a dependency so effect re-runs when navbar position changes
      const _navbarPosition = config().uiCustomization.navbar.position;
      try {
        if (currentPosition === "bottom") {
          this.moveBookmarkBarToBottom();
        } else {
          this.moveBookmarkBarToTop();
        }
      } catch (error: unknown) {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error in bookmark bar position setup:`,
          error,
        );
      }
    });
  }

  private moveBookmarkBarToBottom() {
    if (this.isBookmarkBarAtBottom) {
      return;
    }

    const bookmarkBar = this.personalToolbar;
    const fullscreenWrapper = this.fullscreenWrapper;
    const statusbar = document?.getElementById(
      "nora-statusbar",
    ) as XULElement | null;
    const navbar = this.navBar;

    if (!bookmarkBar) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} PersonalToolbar element not found`,
      );
      return;
    }

    // Find insertion anchor: navbar (if at bottom) > statusbar > fullscreenWrapper
    // Check if navbar is actually at bottom by checking its parent and position
    const navbarAtBottom =
      navbar &&
      navbar.isConnected &&
      (this.isNavbarAtBottom ||
        (navbar.parentElement !== this.navigatorToolbox &&
          navbar.parentElement !== null));

    let insertionAnchor: XULElement | null = null;
    if (navbar && navbar.isConnected && navbarAtBottom) {
      insertionAnchor = navbar;
    } else if (statusbar && statusbar.isConnected) {
      insertionAnchor = statusbar;
    } else if (fullscreenWrapper) {
      insertionAnchor = fullscreenWrapper;
    }

    if (!insertionAnchor) {
      console.warn(
        `${DOMLayoutManager.DEBUG_PREFIX} No valid insertion anchor for bookmark bar placement`,
      );
      return;
    }

    try {
      // Ensure navigator toolbox listener mirroring is installed
      // This is needed because bookmark bar events may be attached to navigator-toolbox
      ensureNavigatorToolboxListenerMirroringInstalled();

      // Save original position
      this.originalBookmarkBarParent = bookmarkBar.parentElement;
      this.originalBookmarkBarNextSibling = bookmarkBar.nextElementSibling;

      // Move bookmark bar to bottom after insertion anchor
      // If navbar is at bottom, place bookmark bar after navbar
      if (navbarAtBottom && insertionAnchor === navbar) {
        navbar.after(bookmarkBar);
      } else {
        insertionAnchor.after(bookmarkBar);
      }
      this.isBookmarkBarAtBottom = true;

      // Set bookmark bar as mirror target if navbar is not at bottom
      // If navbar is also at bottom, navbar will be the mirror target, but we still need
      // to forward bookmark bar events to navigator-toolbox
      if (!navbarAtBottom) {
        setNavigatorToolboxMirrorTarget(bookmarkBar);
      } else {
        // Navbar is at bottom and is the mirror target
        // We need to forward bookmark bar events to navigator-toolbox
        // because bookmark bar is outside navigator-toolbox
        this.attachBookmarkBarEventForwarder(bookmarkBar);
      }
    } catch (error: unknown) {
      console.error(
        `${DOMLayoutManager.DEBUG_PREFIX} Error in moveBookmarkBarToBottom:`,
        error,
      );
    }
  }

  private moveBookmarkBarToTop() {
    if (!this.isBookmarkBarAtBottom) {
      return;
    }

    const bookmarkBar = this.personalToolbar;
    if (!bookmarkBar) {
      return;
    }

    // Restore original position
    if (this.originalBookmarkBarParent) {
      try {
        // Detach bookmark bar event forwarder
        this.detachBookmarkBarEventForwarder();

        // If navbar is at bottom, set it as mirror target, otherwise clear it
        // setNavigatorToolboxMirrorTarget will automatically handle cleanup
        if (this.isNavbarAtBottom) {
          setNavigatorToolboxMirrorTarget(this.navBar);
        } else {
          setNavigatorToolboxMirrorTarget(null);
        }

        if (this.originalBookmarkBarNextSibling) {
          this.originalBookmarkBarParent.insertBefore(
            bookmarkBar,
            this.originalBookmarkBarNextSibling,
          );
        } else {
          this.originalBookmarkBarParent.appendChild(bookmarkBar);
        }
        this.isBookmarkBarAtBottom = false;
        this.originalBookmarkBarParent = null;
        this.originalBookmarkBarNextSibling = null;
      } catch (error: unknown) {
        console.error(
          `${DOMLayoutManager.DEBUG_PREFIX} Error in moveBookmarkBarToTop:`,
          error,
        );
      }
    }
  }

  private attachBookmarkBarEventForwarder(bookmarkBar: XULElement): void {
    if (!bookmarkBar.isConnected) {
      return;
    }

    const toolbox = this.navigatorToolbox;
    if (!toolbox) {
      return;
    }

    // Remove existing forwarder if any
    this.detachBookmarkBarEventForwarder();

    const handler = (event: Event) => {
      // Check if event originated from bookmark bar or its children
      const target = event.target as Element | null;
      const currentTarget = event.currentTarget as Element | null;
      if (
        !target ||
        (target !== bookmarkBar &&
          !bookmarkBar.contains(target) &&
          currentTarget !== bookmarkBar &&
          (!currentTarget || !bookmarkBar.contains(currentTarget)))
      ) {
        return;
      }

      // For command events from bookmark buttons, directly load the URI using gBrowser.loadURI
      // This is simpler and more reliable than event forwarding
      if (event.type === "command") {
        if (handleBookmarkCommandEvent(event)) {
          // Event was handled, don't forward to navigator-toolbox
          return;
        }
      }

      // For other events, forward to navigator-toolbox
      forwardEventToNavigatorToolbox(event, toolbox);
    };

    // Attach event forwarder for all forwarded event types
    // Use capture phase to catch events before they bubble
    // Also listen on the document to catch events from bookmark bar children
    for (const type of FORWARDED_EVENT_TYPES) {
      try {
        bookmarkBar.addEventListener(type, handler, { capture: true });
        // Also listen on document to catch events that might not bubble
        if (document) {
          document.addEventListener(type, handler, { capture: true });
        }
      } catch (error) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} Failed to attach bookmark bar forwarder for ${type}`,
          error,
        );
      }
    }

    this.bookmarkBarForwarderHandler = handler;
  }

  private detachBookmarkBarEventForwarder(): void {
    if (!this.bookmarkBarForwarderHandler) {
      return;
    }

    const bookmarkBar = this.personalToolbar;
    const handler = this.bookmarkBarForwarderHandler;
    for (const type of FORWARDED_EVENT_TYPES) {
      try {
        if (bookmarkBar) {
          bookmarkBar.removeEventListener(type, handler, { capture: true });
        }
        // Also remove from document
        if (document) {
          document.removeEventListener(type, handler, { capture: true });
        }
      } catch (error) {
        console.warn(
          `${DOMLayoutManager.DEBUG_PREFIX} Failed to detach bookmark bar forwarder for ${type}`,
          error,
        );
      }
    }

    this.bookmarkBarForwarderHandler = null;
  }
}
