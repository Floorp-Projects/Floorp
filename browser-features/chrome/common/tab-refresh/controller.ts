/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "./styles.css?inline";
import type {
  HoverReloadBrowser,
  HoverReloadClock,
  HoverReloadControllerOptions,
  HoverReloadDocument,
  HoverReloadMutationObserver,
  HoverReloadPrefObserver,
  HoverReloadPrefs,
  HoverReloadTab,
  HoverReloadTimerHandle,
  HoverReloadUnloadTarget,
} from "./types.ts";

export const ENABLED_PREF = "floorp.tabs.hoverReload.enabled";
export const HOVER_DELAY_MS = 700;
export const ROOT_ATTR = "floorp-hover-reload";
export const STAMP_ATTR = "floorp-show-refresh";
export const GLYPH_CLASS = "floorp-tab-refresh";
export const STYLE_MARKER_ATTR = "data-floorp-tab-refresh-style";

const RELOAD_ICON = "chrome://global/skin/icons/reload.svg";
const CONTROLLER_SLOT = "__floorpHoverReloadController__" as const;

const BLOCKED_CONTROL_SELECTOR = [
  ".tab-close-button",
  ".tab-audio-button",
  ".tab-icon-overlay",
  ".tab-sharing-icon-overlay",
].join(",");

const BLOCKED_TAB_ATTRIBUTES = [
  "activemedia-blocked",
  "attention",
  "busy",
  "closing",
  "collapsed",
  "crashed",
  "grouped",
  "hidden",
  "muted",
  "pending",
  "pictureinpicture",
  "pinned",
  "progress",
  "sharing",
  "soundplaying",
  "titlechanged",
  "usercontextid",
] as const;

const OBSERVED_ATTRIBUTES = [
  ...BLOCKED_TAB_ATTRIBUTES,
  "overflow",
  "overflowing",
] as const;

type GlyphRecord = {
  button: Element;
  cleanup: () => void;
};

type ControllerHost = {
  [CONTROLLER_SLOT]?: HoverReloadController;
};

function defaultClock(): HoverReloadClock {
  return {
    setTimeout: (callback, delay) => globalThis.setTimeout(callback, delay),
    clearTimeout: (handle) => globalThis.clearTimeout(handle),
  };
}

function defaultMutationObserverFactory(
  callback: MutationCallback,
): HoverReloadMutationObserver {
  return new MutationObserver(callback);
}

export class HoverReloadController {
  private readonly browser: HoverReloadBrowser;
  private readonly doc: HoverReloadDocument;
  private readonly prefs: HoverReloadPrefs;
  private readonly clock: HoverReloadClock;
  private readonly createMutationObserver: (
    callback: MutationCallback,
  ) => HoverReloadMutationObserver;
  private readonly unloadTarget: HoverReloadUnloadTarget;
  private readonly hoverDelayMs: number;

  private label: string;
  private started = false;
  private active = false;
  private pointerTab: HoverReloadTab | null = null;
  private blockedControlHovered = false;
  private pendingTab: HoverReloadTab | null = null;
  private visibleTab: HoverReloadTab | null = null;
  private timer: HoverReloadTimerHandle | null = null;
  private glyph: GlyphRecord | null = null;
  private mutationObserver: HoverReloadMutationObserver | null = null;

  private readonly prefObserver: HoverReloadPrefObserver = (
    _subject,
    topic,
    data,
  ) => {
    if (topic === "nsPref:changed" && data === ENABLED_PREF) {
      this.syncEnabledState();
    }
  };

  private readonly onUnload = () => this.destroy();
  private readonly onMouseOver = (event: Event) => {
    this.setBlockedControlHovered(this.isBlockedControlTarget(event.target));
  };
  private readonly onMouseOut = (event: Event) => {
    this.setBlockedControlHovered(
      this.isBlockedControlTarget((event as MouseEvent).relatedTarget),
    );
  };
  private readonly onMouseLeave = () => this.resetInteractionState();
  private readonly onTabHoverStart = (event: Event) => {
    this.pointerTab = this.tabFromEventTarget(event.target);
    this.updateCandidate();
  };
  private readonly onTabHoverEnd = () => this.resetInteractionState();
  private readonly onInteractionCancelled = () => this.resetInteractionState();
  private readonly onEligibilityChanged = () => this.updateCandidate();
  private readonly onVisibilityChange = () => this.resetInteractionState();

  constructor(options: HoverReloadControllerOptions) {
    this.browser = options.browser;
    this.doc = options.document;
    this.prefs = options.prefs;
    this.clock = options.clock ?? defaultClock();
    this.createMutationObserver = options.mutationObserverFactory ??
      defaultMutationObserverFactory;
    this.unloadTarget = options.unloadTarget ?? globalThis;
    this.label = options.label ?? "Reload tab";
    this.hoverDelayMs = options.hoverDelayMs ?? HOVER_DELAY_MS;
  }

  start(): void {
    if (this.started) {
      this.syncEnabledState();
      return;
    }

    this.started = true;
    this.prefs.addObserver(ENABLED_PREF, this.prefObserver);
    this.unloadTarget.addEventListener("unload", this.onUnload, { once: true });
    this.syncEnabledState();
  }

  destroy(): void {
    this.deactivate();
    if (!this.started) {
      return;
    }

    this.started = false;
    this.unloadTarget.removeEventListener("unload", this.onUnload);
    try {
      this.prefs.removeObserver(ENABLED_PREF, this.prefObserver);
    } catch (error) {
      console.error("[tab-refresh] Failed to remove pref observer:", error);
    }
  }

  setLabel(label: string): void {
    this.label = label;
    if (this.glyph) {
      this.applyAccessibleLabel(this.glyph.button);
    }
  }

  private syncEnabledState(): void {
    if (this.prefs.getBoolPref(ENABLED_PREF, false)) {
      this.activate();
    } else {
      this.deactivate();
    }
  }

  private activate(): void {
    if (this.active) {
      return;
    }

    // A previous module instance may have been interrupted before its HMR
    // disposer ran. Purge its inert DOM artifacts before attaching anything.
    this.removeAllArtifacts();
    this.active = true;
    this.doc.documentElement?.setAttribute(ROOT_ATTR, "true");
    this.installStyle();

    const container = this.browser.tabContainer;
    container.addEventListener("TabHoverStart", this.onTabHoverStart);
    container.addEventListener("TabHoverEnd", this.onTabHoverEnd);
    container.addEventListener("mouseover", this.onMouseOver);
    container.addEventListener("mouseout", this.onMouseOut);
    container.addEventListener("mouseleave", this.onMouseLeave);
    container.addEventListener("dragstart", this.onInteractionCancelled, true);
    container.addEventListener("TabClose", this.onInteractionCancelled);
    container.addEventListener("TabSelect", this.onInteractionCancelled);
    container.addEventListener("TabPinned", this.onEligibilityChanged);
    container.addEventListener("TabUnpinned", this.onEligibilityChanged);
    container.addEventListener("TabGrouped", this.onInteractionCancelled);
    container.addEventListener("TabUngrouped", this.onInteractionCancelled);
    container.addEventListener("TabGroupCreate", this.onInteractionCancelled);
    container.addEventListener("TabGroupRemoved", this.onInteractionCancelled);
    container.addEventListener("TabGroupUpdate", this.onInteractionCancelled);
    container.addEventListener("TabGroupExpand", this.onInteractionCancelled);
    container.addEventListener("TabGroupCollapse", this.onInteractionCancelled);
    container.addEventListener("TabAttrModified", this.onEligibilityChanged);
    container.addEventListener("overflow", this.onEligibilityChanged, true);
    container.addEventListener("underflow", this.onEligibilityChanged, true);
    this.unloadTarget.addEventListener("blur", this.onInteractionCancelled);
    this.doc.addEventListener("visibilitychange", this.onVisibilityChange);

    this.mutationObserver = this.createMutationObserver(() => {
      this.updateCandidate();
    });
    this.mutationObserver.observe(container, {
      attributes: true,
      childList: true,
      subtree: true,
      attributeFilter: [...OBSERVED_ATTRIBUTES],
    });
  }

  private deactivate(): void {
    const container = this.browser.tabContainer;
    if (this.active) {
      container.removeEventListener("TabHoverStart", this.onTabHoverStart);
      container.removeEventListener("TabHoverEnd", this.onTabHoverEnd);
      container.removeEventListener("mouseover", this.onMouseOver);
      container.removeEventListener("mouseout", this.onMouseOut);
      container.removeEventListener("mouseleave", this.onMouseLeave);
      container.removeEventListener(
        "dragstart",
        this.onInteractionCancelled,
        true,
      );
      container.removeEventListener("TabClose", this.onInteractionCancelled);
      container.removeEventListener("TabSelect", this.onInteractionCancelled);
      container.removeEventListener("TabPinned", this.onEligibilityChanged);
      container.removeEventListener("TabUnpinned", this.onEligibilityChanged);
      container.removeEventListener("TabGrouped", this.onInteractionCancelled);
      container.removeEventListener(
        "TabUngrouped",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabGroupCreate",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabGroupRemoved",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabGroupUpdate",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabGroupExpand",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabGroupCollapse",
        this.onInteractionCancelled,
      );
      container.removeEventListener(
        "TabAttrModified",
        this.onEligibilityChanged,
      );
      container.removeEventListener(
        "overflow",
        this.onEligibilityChanged,
        true,
      );
      container.removeEventListener(
        "underflow",
        this.onEligibilityChanged,
        true,
      );
      this.unloadTarget.removeEventListener(
        "blur",
        this.onInteractionCancelled,
      );
      this.doc.removeEventListener("visibilitychange", this.onVisibilityChange);
    }

    this.active = false;
    this.mutationObserver?.disconnect();
    this.mutationObserver = null;
    this.resetInteractionState();
    this.removeAllArtifacts();
  }

  private installStyle(): void {
    const style = this.doc.createElement("style");
    style.setAttribute(STYLE_MARKER_ATTR, "true");
    style.textContent = styles;
    this.doc.head?.appendChild(style);
  }

  private removeAllArtifacts(): void {
    this.removeGlyph();
    this.doc.documentElement?.removeAttribute(ROOT_ATTR);
    for (const style of this.doc.querySelectorAll(`[${STYLE_MARKER_ATTR}]`)) {
      style.remove();
    }
    for (const glyph of this.doc.querySelectorAll(`.${GLYPH_CLASS}`)) {
      glyph.remove();
    }
    for (const tab of this.allTabsWithStamp()) {
      tab.removeAttribute(STAMP_ATTR);
    }
  }

  private allTabsWithStamp(): HoverReloadTab[] {
    const tabs = Array.from(this.browser.tabs);
    for (
      const tab of this.browser.tabContainer.querySelectorAll(
        `[${STAMP_ATTR}]`,
      )
    ) {
      if (!tabs.includes(tab)) {
        tabs.push(tab);
      }
    }
    return tabs;
  }

  private tabFromEventTarget(
    target: EventTarget | null,
  ): HoverReloadTab | null {
    if (!(target instanceof Element)) {
      return null;
    }

    const blockedControl = target.closest(BLOCKED_CONTROL_SELECTOR);
    if (blockedControl && !target.closest(`.${GLYPH_CLASS}`)) {
      return null;
    }

    const tab = target.closest(".tabbrowser-tab");
    if (!tab || !this.browser.tabContainer.contains(tab)) {
      return null;
    }
    return tab;
  }

  private isBlockedControlTarget(target: EventTarget | null): boolean {
    return target instanceof Element &&
      this.browser.tabContainer.contains(target) &&
      target.closest(BLOCKED_CONTROL_SELECTOR) !== null;
  }

  private setBlockedControlHovered(blocked: boolean): void {
    if (this.blockedControlHovered === blocked) {
      return;
    }
    this.blockedControlHovered = blocked;
    this.updateCandidate();
  }

  private updateCandidate(): void {
    if (!this.active) {
      this.clearCandidate();
      return;
    }

    const next = this.pointerTab && !this.blockedControlHovered &&
        this.isEligible(this.pointerTab)
      ? this.pointerTab
      : null;

    if (
      (next === null && this.pendingTab === null && this.visibleTab === null) ||
      (next !== null &&
        (next === this.pendingTab || next === this.visibleTab))
    ) {
      return;
    }

    this.clearCandidate();
    if (!next) {
      return;
    }

    this.pendingTab = next;
    this.timer = this.clock.setTimeout(() => {
      this.timer = null;
      this.pendingTab = null;
      if (
        this.active &&
        !this.blockedControlHovered &&
        this.isEligible(next) &&
        this.pointerTab === next
      ) {
        this.showGlyph(next);
      }
    }, this.hoverDelayMs);
  }

  private clearCandidate(): void {
    if (this.timer !== null) {
      this.clock.clearTimeout(this.timer);
      this.timer = null;
    }
    this.pendingTab = null;
    this.removeGlyph();
  }

  private resetInteractionState(): void {
    this.pointerTab = null;
    this.blockedControlHovered = false;
    this.clearCandidate();
  }

  private isEligible(tab: HoverReloadTab): boolean {
    if (!tab.isConnected || !this.browser.tabContainer.contains(tab)) {
      return false;
    }
    if (
      tab.closing === true ||
      BLOCKED_TAB_ATTRIBUTES.some((attribute) => tab.hasAttribute(attribute))
    ) {
      return false;
    }
    if (tab.closest("tab-group") || tab.hasAttribute("grouped")) {
      return false;
    }

    const content = tab.querySelector(".tab-content");
    if (
      !content ||
      content.hasAttribute("attention") ||
      content.hasAttribute("titlechanged")
    ) {
      return false;
    }

    const container = this.browser.tabContainer;
    if (
      container.getAttribute("overflow") === "true" ||
      container.hasAttribute("overflowing") ||
      container.querySelector("#tabbrowser-arrowscrollbox[overflowing]")
    ) {
      return false;
    }
    return true;
  }

  private showGlyph(tab: HoverReloadTab): void {
    this.removeGlyph();
    const content = tab.querySelector(".tab-content");
    if (!content || !this.isEligible(tab)) {
      return;
    }

    // Match Firefox's native tab-close accessibility model: expose a named
    // button to accessibility APIs without adding another stop inside the
    // ARIA tab sequence. Keyboard users retain the tab's native reload
    // shortcut/context-menu paths instead of entering a nested control.
    const button = this.doc.createXULElement?.("image") ??
      this.doc.createElement("span");
    button.classList.add(GLYPH_CLASS);
    button.setAttribute("src", RELOAD_ICON);
    button.setAttribute("role", "button");
    button.setAttribute("keyNav", "false");
    button.setAttribute("tabindex", "-1");
    this.applyAccessibleLabel(button);

    const suppressMouseActivation = (event: Event) => {
      event.preventDefault();
      event.stopImmediatePropagation();
    };
    const click = (event: Event) => {
      const mouseEvent = event as MouseEvent;
      suppressMouseActivation(event);
      if (typeof mouseEvent.button === "number" && mouseEvent.button !== 0) {
        return;
      }
      this.reload(tab, button);
    };
    button.addEventListener("pointerdown", suppressMouseActivation);
    button.addEventListener("mousedown", suppressMouseActivation);
    button.addEventListener("mouseup", suppressMouseActivation);
    button.addEventListener("click", click);
    button.addEventListener("dblclick", suppressMouseActivation);

    const cleanup = () => {
      button.removeEventListener("pointerdown", suppressMouseActivation);
      button.removeEventListener("mousedown", suppressMouseActivation);
      button.removeEventListener("mouseup", suppressMouseActivation);
      button.removeEventListener("click", click);
      button.removeEventListener("dblclick", suppressMouseActivation);
      button.remove();
    };

    const closeButton = content.querySelector(".tab-close-button");
    content.insertBefore(button, closeButton);
    tab.setAttribute(STAMP_ATTR, "true");
    this.visibleTab = tab;
    this.glyph = { button, cleanup };
  }

  private applyAccessibleLabel(button: Element): void {
    button.setAttribute("aria-label", this.label);
    button.setAttribute("tooltiptext", this.label);
    button.setAttribute("title", this.label);
  }

  private reload(tab: HoverReloadTab, button: Element): void {
    if (
      !this.active ||
      this.blockedControlHovered ||
      this.glyph?.button !== button ||
      !button.isConnected ||
      !this.isEligible(tab)
    ) {
      return;
    }

    try {
      this.browser.reloadTab(tab);
    } catch (error) {
      console.error("[tab-refresh] Failed to reload tab:", error);
    }
  }

  private removeGlyph(): void {
    this.glyph?.cleanup();
    this.glyph = null;
    this.visibleTab?.removeAttribute(STAMP_ATTR);
    this.visibleTab = null;
  }
}

export function installHoverReloadController(
  options: HoverReloadControllerOptions,
  host: object = globalThis,
): HoverReloadController {
  const controllerHost = host as ControllerHost;
  controllerHost[CONTROLLER_SLOT]?.destroy();
  const controller = new HoverReloadController(options);
  controllerHost[CONTROLLER_SLOT] = controller;
  controller.start();
  return controller;
}

export function uninstallHoverReloadController(
  controller: HoverReloadController,
  host: object = globalThis,
): void {
  const controllerHost = host as ControllerHost;
  if (controllerHost[CONTROLLER_SLOT] === controller) {
    delete controllerHost[CONTROLLER_SLOT];
  }
  controller.destroy();
}
