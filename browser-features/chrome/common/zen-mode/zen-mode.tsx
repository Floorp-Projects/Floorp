/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { type Accessor, createSignal, type Setter } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import zenModeStyle from "./zen-mode.css?inline";

export const ZEN_MODE_PREF = "floorp.zenmode.enabled";
export const ZEN_MODE_STYLE_ID = "floorp-zen-mode-style";

const EDGE_THRESHOLD = 10;
export const ZEN_MODE_HIDE_DELAY_MS = 500;

const ZEN_MODE_ATTRIBUTES = [
  "zenmode",
  "zenmode-reveal-top",
  "zenmode-reveal-bottom",
  "zenmode-reveal-side",
  "zenmode-rebase",
] as const;

const ZEN_MODE_CSS_VARIABLES = [
  "--zenmode-toolbox-height",
  "--zenmode-sidebar-width",
  "--zenmode-selectbox-width",
  "--zenmode-statusbar-height",
] as const;

const MEASURED_ELEMENTS = [
  {
    id: "panel-sidebar-box",
    property: "--zenmode-sidebar-width",
  },
  {
    id: "panel-sidebar-select-box",
    property: "--zenmode-selectbox-width",
  },
] as const;

type MeasuredElement = (typeof MEASURED_ELEMENTS)[number];

type BrowserWindowWithToolbox = Window & {
  gNavToolbox?: EventTarget | null;
};

type BrowserWindowWithZenController = Window & {
  __floorpZenModeController?: ZenModeController;
};

type ControllerDestroyedCallback = (controller: ZenModeController) => void;

function readPersistedSeed(): boolean {
  try {
    return typeof Services !== "undefined"
      ? Services.prefs.getBoolPref(ZEN_MODE_PREF, false)
      : false;
  } catch {
    return false;
  }
}

function persistSeed(enabled: boolean): void {
  try {
    Services.prefs.setBoolPref(ZEN_MODE_PREF, enabled);
  } catch (error) {
    console.error("[zen-mode] Failed to persist Zen mode seed:", error);
  }
}

/**
 * Owns every piece of Zen state and chrome integration for one browser window.
 * The preference is read once, at construction, and is never observed.
 */
export class ZenModeController {
  public readonly enabled: Accessor<boolean>;

  private readonly setEnabledSignal: Setter<boolean>;
  private readonly document: Document;
  private readonly root: HTMLElement;
  private readonly onDestroyed: ControllerDestroyedCallback;

  private destroyed = false;
  private styleElement: HTMLStyleElement | null = null;
  private domObserver: MutationObserver | null = null;
  private resizeObserver: ResizeObserver | null = null;
  private readonly observedElements = new Map<string, Element>();
  private customizationTarget: EventTarget | null = null;

  private topHideTimer: number | null = null;
  private bottomHideTimer: number | null = null;
  private sideHideTimer: number | null = null;
  private rebaseRafOne: number | null = null;
  private rebaseRafTwo: number | null = null;

  constructor(
    public readonly targetWindow: Window,
    onDestroyed: ControllerDestroyedCallback,
  ) {
    const targetDocument = targetWindow.document;
    const root = targetDocument?.documentElement as HTMLElement | null;
    if (!targetDocument || !root) {
      throw new Error("Zen mode requires a browser window document");
    }

    this.document = targetDocument;
    this.root = root;
    this.onDestroyed = onDestroyed;

    const [enabled, setEnabled] = createSignal(readPersistedSeed());
    this.enabled = enabled;
    this.setEnabledSignal = setEnabled;

    try {
      this.initialize();
    } catch (error) {
      this.destroy();
      throw error;
    }
  }

  public isDestroyed(): boolean {
    return this.destroyed;
  }

  /** Toggle from a menu, toolbar button, or mouse gesture. */
  public toggleFromUser(): boolean {
    return this.setEnabledFromUser(!this.enabled());
  }

  /** Set from an explicit user action and persist only the future-window seed. */
  public setEnabledFromUser(enabled: boolean): boolean {
    if (this.destroyed) {
      return this.enabled();
    }

    this.setLocalEnabled(enabled);
    persistSeed(enabled);
    return enabled;
  }

  /** Toolbar customization is intentionally local and non-persistent. */
  public disableForCustomization(): void {
    if (!this.destroyed) {
      this.setLocalEnabled(false);
    }
  }

  public destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;

    this.targetWindow.removeEventListener("mousemove", this.handleMouseMove);
    this.targetWindow.removeEventListener("unload", this.handleWindowUnload);
    this.document.removeEventListener("focusin", this.handleUrlbarFocusIn);
    this.document.removeEventListener("focusout", this.handleUrlbarFocusOut);

    this.setCustomizationTarget(null);

    this.domObserver?.disconnect();
    this.domObserver = null;
    this.resizeObserver?.disconnect();
    this.resizeObserver = null;
    this.observedElements.clear();

    this.clearTopTimer();
    this.clearBottomTimer();
    this.clearSideTimer();
    this.cancelRebaseFrames();

    for (const attribute of ZEN_MODE_ATTRIBUTES) {
      this.root.removeAttribute(attribute);
    }
    for (const property of ZEN_MODE_CSS_VARIABLES) {
      this.root.style.removeProperty(property);
    }

    this.styleElement?.remove();
    this.styleElement = null;

    this.onDestroyed(this);
  }

  private initialize(): void {
    this.ensureStyleElement();

    this.targetWindow.addEventListener("mousemove", this.handleMouseMove);
    this.targetWindow.addEventListener("unload", this.handleWindowUnload, {
      once: true,
    });
    this.document.addEventListener("focusin", this.handleUrlbarFocusIn);
    this.document.addEventListener("focusout", this.handleUrlbarFocusOut);

    const ResizeObserverConstructor = this.targetWindow.ResizeObserver;
    if (typeof ResizeObserverConstructor === "function") {
      this.resizeObserver = new ResizeObserverConstructor(
        this.handleMeasuredElementResize,
      );
    }

    const MutationObserverConstructor = this.targetWindow.MutationObserver;
    if (typeof MutationObserverConstructor === "function") {
      const observer = new MutationObserverConstructor(
        this.handleDocumentMutation,
      );
      observer.observe(this.root, {
        attributes: true,
        attributeFilter: ["open"],
        childList: true,
        subtree: true,
      });
      this.domObserver = observer;
    }

    this.syncOwnedElements();
    this.applyEnabledState(this.enabled());
  }

  private setLocalEnabled(enabled: boolean): void {
    this.setEnabledSignal(enabled);
    this.applyEnabledState(enabled);
  }

  private applyEnabledState(enabled: boolean): void {
    if (enabled) {
      this.measureCurrentElements(false);
      this.root.setAttribute("zenmode", "");
      return;
    }

    this.clearTopTimer();
    this.clearBottomTimer();
    this.clearSideTimer();
    this.cancelRebaseFrames();
    this.root.removeAttribute("zenmode");
    this.root.removeAttribute("zenmode-reveal-top");
    this.root.removeAttribute("zenmode-reveal-bottom");
    this.root.removeAttribute("zenmode-reveal-side");
    this.root.removeAttribute("zenmode-rebase");
  }

  private ensureStyleElement(): void {
    if (this.styleElement?.isConnected) {
      return;
    }

    const existing = this.document.getElementById(ZEN_MODE_STYLE_ID);
    if (existing?.localName === "style") {
      this.styleElement = existing as HTMLStyleElement;
      this.styleElement.textContent = zenModeStyle;
      return;
    }

    const style = this.document.createElement("style");
    style.id = ZEN_MODE_STYLE_ID;
    style.setAttribute("data-floorp-zen-mode-owned", "true");
    style.textContent = zenModeStyle;
    (this.document.head ?? this.root).appendChild(style);
    this.styleElement = style;
  }

  private readonly handleWindowUnload = (): void => {
    destroyZenModeForWindow(this.targetWindow, this);
  };

  private readonly handleCustomizationStarting = (): void => {
    this.disableForCustomization();
  };

  private setCustomizationTarget(target: EventTarget | null): void {
    if (this.customizationTarget === target) {
      return;
    }

    this.customizationTarget?.removeEventListener(
      "customizationstarting",
      this.handleCustomizationStarting,
    );
    this.customizationTarget = target;
    this.customizationTarget?.addEventListener(
      "customizationstarting",
      this.handleCustomizationStarting,
    );
  }

  private syncOwnedElements(): void {
    if (this.destroyed) {
      return;
    }

    this.ensureStyleElement();

    const explicitToolbox = (this.targetWindow as BrowserWindowWithToolbox)
      .gNavToolbox;
    const toolbox = explicitToolbox ??
      this.document.getElementById("navigator-toolbox");
    this.setCustomizationTarget(toolbox);

    for (const measured of MEASURED_ELEMENTS) {
      const previous = this.observedElements.get(measured.id) ?? null;
      const current = this.document.getElementById(measured.id);

      if (previous && previous !== current) {
        this.resizeObserver?.unobserve(previous);
        this.observedElements.delete(measured.id);
      }

      if (current && previous !== current) {
        this.observedElements.set(measured.id, current);
        this.resizeObserver?.observe(current);
      }
    }

    this.measureCurrentElements(this.enabled());
  }

  private readonly handleDocumentMutation: MutationCallback = (mutations) => {
    if (this.destroyed) {
      return;
    }

    let shouldSyncElements = false;
    for (const mutation of mutations) {
      if (mutation.type === "childList") {
        shouldSyncElements = true;
        continue;
      }

      if (
        mutation.type === "attributes" &&
        mutation.attributeName === "open" &&
        mutation.target === this.document.getElementById("urlbar") &&
        !(mutation.target as Element).hasAttribute("open") &&
        this.enabled()
      ) {
        this.scheduleTopHide();
      }
    }

    if (shouldSyncElements) {
      this.syncOwnedElements();
    }
  };

  private readonly handleMeasuredElementResize: ResizeObserverCallback = (
    entries,
  ) => {
    if (this.destroyed) {
      return;
    }

    const elements = entries
      .map((entry) => entry.target)
      .filter((element) => element.ownerDocument === this.document);
    this.applyMeasurements(elements, this.enabled());
  };

  private measureCurrentElements(rebase: boolean): void {
    const elements = MEASURED_ELEMENTS.map((measured) =>
      this.document.getElementById(measured.id)
    ).filter((element): element is HTMLElement => element !== null);
    this.applyMeasurements(elements, rebase);
  }

  private applyMeasurements(elements: Element[], rebase: boolean): void {
    const updates: Array<{ measured: MeasuredElement; value: string }> = [];

    for (const element of elements) {
      const measured = MEASURED_ELEMENTS.find(({ id }) => id === element.id);
      if (!measured || this.document.getElementById(measured.id) !== element) {
        continue;
      }

      const width = element.getBoundingClientRect().width;
      if (!Number.isFinite(width) || width <= 0) {
        continue;
      }

      const value = `${width}px`;
      if (this.root.style.getPropertyValue(measured.property) !== value) {
        updates.push({ measured, value });
      }
    }

    if (updates.length === 0) {
      return;
    }

    if (rebase) {
      this.beginRebase();
    }
    for (const update of updates) {
      this.root.style.setProperty(update.measured.property, update.value);
    }
  }

  private beginRebase(): void {
    if (!this.enabled() || this.destroyed) {
      return;
    }

    this.cancelRebaseFrames(false);
    this.root.setAttribute("zenmode-rebase", "");
    this.rebaseRafOne = this.targetWindow.requestAnimationFrame(() => {
      this.rebaseRafOne = null;
      if (this.destroyed) {
        return;
      }
      this.rebaseRafTwo = this.targetWindow.requestAnimationFrame(() => {
        this.rebaseRafTwo = null;
        if (!this.destroyed) {
          this.root.removeAttribute("zenmode-rebase");
        }
      });
    });
  }

  private cancelRebaseFrames(removeAttribute = true): void {
    if (this.rebaseRafOne !== null) {
      this.targetWindow.cancelAnimationFrame(this.rebaseRafOne);
      this.rebaseRafOne = null;
    }
    if (this.rebaseRafTwo !== null) {
      this.targetWindow.cancelAnimationFrame(this.rebaseRafTwo);
      this.rebaseRafTwo = null;
    }
    if (removeAttribute) {
      this.root.removeAttribute("zenmode-rebase");
    }
  }

  private isOwnedTopPopupOpen(): boolean {
    const selectors = [
      "#navigator-toolbox menupopup[open]",
      "#navigator-toolbox panel[open]",
      "#mainPopupSet menupopup[open]",
      "#mainPopupSet panel[open]",
    ];
    return selectors.some((selector) => {
      const popup = this.document.querySelector(selector) as Element | null;
      return popup?.ownerDocument === this.document;
    });
  }

  private isUrlbarOpen(): boolean {
    return this.document.getElementById("urlbar")?.hasAttribute("open") ??
      false;
  }

  private isOrContainsUrlbar(target: EventTarget | null): boolean {
    const node = target as Node | null;
    if (!node || node.ownerDocument !== this.document) {
      return false;
    }

    const urlbar = this.document.getElementById("urlbar");
    return urlbar !== null && (urlbar === node || urlbar.contains(node));
  }

  private readonly tryHideTop = (): void => {
    if (!this.enabled() || this.destroyed) {
      this.topHideTimer = null;
      return;
    }

    if (this.isOwnedTopPopupOpen() || this.isUrlbarOpen()) {
      this.topHideTimer = this.targetWindow.setTimeout(
        this.tryHideTop,
        ZEN_MODE_HIDE_DELAY_MS,
      );
      return;
    }

    this.root.removeAttribute("zenmode-reveal-top");
    this.topHideTimer = null;
  };

  private scheduleTopHide(): void {
    this.clearTopTimer();
    this.topHideTimer = this.targetWindow.setTimeout(
      this.tryHideTop,
      ZEN_MODE_HIDE_DELAY_MS,
    );
  }

  private readonly handleMouseMove = (event: MouseEvent): void => {
    if (!this.enabled() || this.destroyed) {
      return;
    }

    const { clientX, clientY } = event;
    const windowWidth = this.targetWindow.innerWidth;
    const windowHeight = this.targetWindow.innerHeight;

    if (clientY <= EDGE_THRESHOLD) {
      this.clearTopTimer();
      this.root.setAttribute("zenmode-reveal-top", "");
    } else if (this.root.hasAttribute("zenmode-reveal-top")) {
      const toolbox = this.document.getElementById("navigator-toolbox");
      if (!toolbox || clientY > toolbox.getBoundingClientRect().bottom) {
        this.scheduleTopHide();
      } else {
        this.clearTopTimer();
      }
    }

    if (clientY >= windowHeight - EDGE_THRESHOLD) {
      this.clearBottomTimer();
      this.root.setAttribute("zenmode-reveal-bottom", "");
    } else if (this.root.hasAttribute("zenmode-reveal-bottom")) {
      const statusbar = this.document.getElementById("nora-statusbar");
      if (!statusbar || clientY < statusbar.getBoundingClientRect().top) {
        this.clearBottomTimer();
        this.bottomHideTimer = this.targetWindow.setTimeout(() => {
          this.root.removeAttribute("zenmode-reveal-bottom");
          this.bottomHideTimer = null;
        }, ZEN_MODE_HIDE_DELAY_MS);
      } else {
        this.clearBottomTimer();
      }
    }

    if (clientX <= EDGE_THRESHOLD || clientX >= windowWidth - EDGE_THRESHOLD) {
      this.clearSideTimer();
      this.root.setAttribute("zenmode-reveal-side", "");
    } else if (this.root.hasAttribute("zenmode-reveal-side")) {
      const panelSidebar = this.document.getElementById("panel-sidebar-box");
      const panelSelectBox = this.document.getElementById(
        "panel-sidebar-select-box",
      );
      const insideSidebar = [panelSidebar, panelSelectBox].some((element) => {
        if (!element) {
          return false;
        }
        const rect = element.getBoundingClientRect();
        return clientX >= rect.left && clientX <= rect.right;
      });

      if (!insideSidebar) {
        this.clearSideTimer();
        this.sideHideTimer = this.targetWindow.setTimeout(() => {
          this.root.removeAttribute("zenmode-reveal-side");
          this.sideHideTimer = null;
        }, ZEN_MODE_HIDE_DELAY_MS);
      } else {
        this.clearSideTimer();
      }
    }
  };

  private readonly handleUrlbarFocusIn = (event: FocusEvent): void => {
    if (!this.enabled() || !this.isOrContainsUrlbar(event.target)) {
      return;
    }
    this.clearTopTimer();
    this.root.setAttribute("zenmode-reveal-top", "");
  };

  private readonly handleUrlbarFocusOut = (event: FocusEvent): void => {
    if (!this.enabled() || !this.isOrContainsUrlbar(event.target)) {
      return;
    }
    this.scheduleTopHide();
  };

  private clearTopTimer(): void {
    if (this.topHideTimer !== null) {
      this.targetWindow.clearTimeout(this.topHideTimer);
      this.topHideTimer = null;
    }
  }

  private clearBottomTimer(): void {
    if (this.bottomHideTimer !== null) {
      this.targetWindow.clearTimeout(this.bottomHideTimer);
      this.bottomHideTimer = null;
    }
  }

  private clearSideTimer(): void {
    if (this.sideHideTimer !== null) {
      this.targetWindow.clearTimeout(this.sideHideTimer);
      this.sideHideTimer = null;
    }
  }
}

const zenModeControllers = new Map<Window, ZenModeController>();

function getAttachedWindowController(
  win: Window,
): ZenModeController | undefined {
  const controller = (win as BrowserWindowWithZenController)
    .__floorpZenModeController;
  if (!controller || controller.isDestroyed()) {
    return undefined;
  }
  return controller;
}

export function attachZenModeToWindow(win: Window): ZenModeController | null {
  if (!win || win.closed === true || !win.document?.documentElement) {
    return null;
  }

  // Mouse gestures may be owned by the first browser window's module context.
  // Keep the authoritative controller on the addressed Window so another
  // context routes to the same signal instead of creating a duplicate.
  const attached = getAttachedWindowController(win);
  if (attached) {
    return attached;
  }

  const existing = zenModeControllers.get(win);
  if (existing && !existing.isDestroyed()) {
    return existing;
  }

  let controller: ZenModeController;
  try {
    controller = new ZenModeController(win, (destroyedController) => {
      if (zenModeControllers.get(win) === destroyedController) {
        zenModeControllers.delete(win);
      }
      const markedWindow = win as BrowserWindowWithZenController;
      if (markedWindow.__floorpZenModeController === destroyedController) {
        try {
          delete markedWindow.__floorpZenModeController;
        } catch {
          markedWindow.__floorpZenModeController = undefined;
        }
      }
    });
  } catch (error) {
    console.error("[zen-mode] Failed to attach window controller:", error);
    return null;
  }

  zenModeControllers.set(win, controller);
  (win as BrowserWindowWithZenController).__floorpZenModeController =
    controller;
  return controller;
}

export function getZenModeController(
  win: Window,
): ZenModeController | undefined {
  const controller = getAttachedWindowController(win) ??
    zenModeControllers.get(win);
  return controller?.isDestroyed() ? undefined : controller;
}

export function toggleZenModeForWindow(win: Window): boolean | null {
  return attachZenModeToWindow(win)?.toggleFromUser() ?? null;
}

export function destroyZenModeForWindow(
  win: Window,
  expectedController?: ZenModeController,
): void {
  const controller = getAttachedWindowController(win) ??
    zenModeControllers.get(win);
  if (
    !controller || (expectedController && controller !== expectedController)
  ) {
    return;
  }
  controller.destroy();
}

export function ZenModeMenuElement(props: { targetWindow: Window }) {
  const controller = attachZenModeToWindow(props.targetWindow);
  const [label, setLabel] = createSignal(
    i18next.t("zen-mode.menu-label", { defaultValue: "Toggle Zen Mode" }),
  );

  addI18nObserver(() => {
    setLabel(
      i18next.t("zen-mode.menu-label", { defaultValue: "Toggle Zen Mode" }),
    );
  });

  const handleCommand = (event?: Event) => {
    const menuitem = event?.currentTarget as Element | null;
    const owningWindow = menuitem?.ownerDocument?.defaultView as Window | null;
    toggleZenModeForWindow(owningWindow ?? props.targetWindow);
  };

  return (
    <xul:menuitem
      label={label()}
      type="checkbox"
      id="toggle_zenmode"
      checked={controller?.enabled() || undefined}
      onCommand={handleCommand}
      accesskey="Z"
    />
  );
}
