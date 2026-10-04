// SPDX-License-Identifier: MPL-2.0

import {
  batch,
  createEffect,
  createRoot,
  createSignal,
  getOwner,
  on,
  onCleanup,
  runWithOwner,
} from "solid-js";
import {
  isFloating,
  isFloatingDragging,
  isPanelSidebarEnabled,
  isPanelSidebarHoverOpen,
  isPanelSidebarHoverPreview,
  isPanelSidebarOverlay,
  isPanelSidebarResizing,
  panelSidebarConfig,
  selectedPanelId,
  setIsPanelSidebarHoverOpen,
  setIsPanelSidebarHoverPreview,
  setSelectedPanelId,
} from "../data/data.ts";
import type { CPanelSidebar } from "./panel-sidebar.tsx";

export const PANEL_HOVER_OPEN_DELAY = 150;
export const PANEL_HOVER_CLOSE_DELAY = 300;

/** Anchored hover previews never change the page's layout or unload on leave. */
export class PanelSidebarHover {
  private openTimer: ReturnType<typeof setTimeout> | undefined;
  private closeTimer: ReturnType<typeof setTimeout> | undefined;
  private readonly pointerState = createSignal(false);
  private readonly draggingState = createSignal(false);
  private readonly popups = new Set<Element>();

  private suppressed(): boolean {
    const root = document.documentElement;
    const hiddenChrome = root.getAttribute("chromehidden")?.split(/\s+/) ?? [];
    return root.hasAttribute("inDOMFullscreen") ||
      (root.hasAttribute("inFullscreen") &&
        !root.hasAttribute("macOSNativeFullscreen")) ||
      root.hasAttribute("invisibleBMS") ||
      hiddenChrome.some((part) =>
        ["toolbar", "directories", "extrachrome"].includes(part)
      ) ||
      document.getElementById("browser")?.hasAttribute("data-is-child") ===
        true;
  }

  constructor(private readonly controller: CPanelSidebar) {
    const owner = getOwner();
    if (owner) runWithOwner(owner, () => this.init());
    else createRoot(() => this.init());
  }

  private enabled(): boolean {
    return !this.suppressed() && isPanelSidebarEnabled() &&
      isPanelSidebarOverlay() &&
      panelSidebarConfig().openOnHover === true;
  }

  private init(): void {
    document.addEventListener("mouseover", this.onMouseOver, true);
    document.addEventListener("mouseout", this.onMouseOut, true);
    document.addEventListener("focusin", this.onFocusIn, true);
    document.addEventListener("focusout", this.onFocusOut, true);
    document.addEventListener("popupshown", this.onPopupShown, true);
    document.addEventListener("popuphidden", this.onPopupHidden, true);
    document.addEventListener("dragstart", this.onDragStart, true);
    document.addEventListener("dragend", this.onDragEnd, true);
    document.addEventListener("keydown", this.onKeyDown, true);
    globalThis.addEventListener("blur", this.onWindowBlur);
    const visibilityObserver = new MutationObserver(() => {
      if (!this.suppressed()) return;
      this.cancelOpen();
      this.cancelClose();
      this.pointerState[1](false);
      if (isPanelSidebarHoverPreview()) setIsPanelSidebarHoverOpen(false);
    });
    visibilityObserver.observe(document.documentElement, {
      attributes: true,
      attributeFilter: [
        "inDOMFullscreen",
        "inFullscreen",
        "macOSNativeFullscreen",
        "chromehidden",
        "invisibleBMS",
      ],
    });
    const browser = document.getElementById("browser");
    if (browser) {
      visibilityObserver.observe(browser, {
        attributes: true,
        attributeFilter: ["data-is-child"],
      });
    }

    createEffect(on(
      () => [
        isPanelSidebarEnabled(),
        isFloating(),
        panelSidebarConfig().overlay,
        panelSidebarConfig().openOnHover,
        panelSidebarConfig().position_start,
      ],
      () => {
        this.cancelOpen();
        this.cancelClose();
        // A setting/mode change must not turn a temporary preview into a
        // manually opened docked or floating panel.
        if (isPanelSidebarHoverPreview()) {
          batch(() => {
            setSelectedPanelId(null);
            setIsPanelSidebarHoverPreview(false);
            setIsPanelSidebarHoverOpen(false);
          });
        }
      },
      { defer: true },
    ));
    createEffect(on(
      [selectedPanelId, isPanelSidebarHoverPreview],
      () => {
        this.cancelOpen();
        if (selectedPanelId() === null) {
          batch(() => {
            setIsPanelSidebarHoverPreview(false);
            setIsPanelSidebarHoverOpen(false);
          });
        }
      },
      { defer: true },
    ));
    createEffect(on(
      [isPanelSidebarResizing, isFloatingDragging],
      () => this.scheduleClose(),
      { defer: true },
    ));
    onCleanup(() => {
      this.cancelOpen();
      this.cancelClose();
      visibilityObserver.disconnect();
      this.popups.clear();
      document.removeEventListener("mouseover", this.onMouseOver, true);
      document.removeEventListener("mouseout", this.onMouseOut, true);
      document.removeEventListener("focusin", this.onFocusIn, true);
      document.removeEventListener("focusout", this.onFocusOut, true);
      document.removeEventListener("popupshown", this.onPopupShown, true);
      document.removeEventListener("popuphidden", this.onPopupHidden, true);
      document.removeEventListener("dragstart", this.onDragStart, true);
      document.removeEventListener("dragend", this.onDragEnd, true);
      document.removeEventListener("keydown", this.onKeyDown, true);
      globalThis.removeEventListener("blur", this.onWindowBlur);
    });
  }

  private inRegion(target: EventTarget | null): boolean {
    if (!(target instanceof Node)) return false;
    return document.getElementById("panel-sidebar-box")?.contains(target) ===
        true ||
      document.getElementById("panel-sidebar-select-box")?.contains(target) ===
        true;
  }

  private panelId(target: EventTarget | null): string | undefined {
    if (!(target instanceof Element)) return undefined;
    const button = target.closest(".panel-sidebar-panel[data-panel-id]");
    if (
      !document.getElementById("panel-sidebar-select-box")?.contains(button)
    ) {
      return undefined;
    }
    return button?.getAttribute("data-panel-id") ?? undefined;
  }

  private onMouseOver = (event: MouseEvent): void => {
    if (!this.enabled()) return;
    const inside = this.inRegion(event.target);
    this.pointerState[1](inside);
    if (!inside) {
      this.cancelOpen();
      this.scheduleClose();
      return;
    }
    this.cancelClose();
    const panelId = this.panelId(event.target);
    if (panelId && panelId !== this.panelId(event.relatedTarget)) {
      this.scheduleOpen(panelId);
    }
  };

  private onMouseOut = (event: MouseEvent): void => {
    if (!this.enabled() || !this.inRegion(event.target)) return;
    if (this.panelId(event.target) !== this.panelId(event.relatedTarget)) {
      this.cancelOpen();
    }
    if (this.inRegion(event.relatedTarget)) return;
    this.pointerState[1](false);
    this.scheduleClose();
  };

  private scheduleOpen(panelId: string): void {
    this.cancelOpen();
    // Manual opening pins the panel until the user closes it.
    if (selectedPanelId() && !isPanelSidebarHoverPreview()) return;
    this.openTimer = setTimeout(() => {
      this.openTimer = undefined;
      if (
        !this.enabled() || !this.pointerState[0]() || this.draggingState[0]() ||
        (selectedPanelId() && !isPanelSidebarHoverPreview()) ||
        !this.controller.getPanelData(panelId)
      ) return;
      batch(() => {
        setIsPanelSidebarHoverPreview(true);
        setIsPanelSidebarHoverOpen(true);
        this.controller.openPanel(panelId);
      });
    }, PANEL_HOVER_OPEN_DELAY);
  }

  private protectedInteraction(): boolean {
    return this.draggingState[0]() || isPanelSidebarResizing() ||
      isFloatingDragging() || this.inRegion(document.activeElement) ||
      this.hasPopup();
  }

  private hasPopup(): boolean {
    return this.popups.size > 0 ||
      document.querySelector(
          "panel[panelopen], menupopup[state='open'], menupopup[state='showing']",
        ) !== null;
  }

  private scheduleClose(): void {
    this.cancelClose();
    if (
      !this.enabled() || this.pointerState[0]() ||
      !isPanelSidebarHoverPreview() || !isPanelSidebarHoverOpen()
    ) return;
    this.closeTimer = setTimeout(() => {
      this.closeTimer = undefined;
      if (
        this.enabled() && !this.pointerState[0]() &&
        isPanelSidebarHoverPreview() && !this.protectedInteraction()
      ) {
        setIsPanelSidebarHoverOpen(false);
      }
    }, PANEL_HOVER_CLOSE_DELAY);
  }

  private cancelOpen(): void {
    if (this.openTimer !== undefined) clearTimeout(this.openTimer);
    this.openTimer = undefined;
  }

  private cancelClose(): void {
    if (this.closeTimer !== undefined) clearTimeout(this.closeTimer);
    this.closeTimer = undefined;
  }

  private onFocusIn = (event: FocusEvent): void => {
    if (this.inRegion(event.target)) this.cancelClose();
    else this.scheduleClose();
  };
  private onFocusOut = (): void => this.scheduleClose();
  private onPopupShown = (event: Event): void => {
    if (
      event.target instanceof Element && event.target.localName !== "tooltip"
    ) {
      this.popups.add(event.target);
      this.cancelClose();
    }
  };
  private onPopupHidden = (event: Event): void => {
    if (event.target instanceof Element) this.popups.delete(event.target);
    this.scheduleClose();
  };
  private onDragStart = (): void => {
    this.draggingState[1](true);
    this.cancelOpen();
    this.cancelClose();
  };
  private onDragEnd = (): void => {
    this.draggingState[1](false);
    this.scheduleClose();
  };
  private onWindowBlur = (): void => {
    this.cancelOpen();
    this.pointerState[1](false);
    this.scheduleClose();
  };
  private onKeyDown = (event: KeyboardEvent): void => {
    if (
      event.key === "Escape" && !event.defaultPrevented &&
      isPanelSidebarHoverPreview() && isPanelSidebarHoverOpen() &&
      !this.hasPopup()
    ) {
      this.cancelClose();
      setIsPanelSidebarHoverOpen(false);
      if (this.inRegion(document.activeElement)) {
        (globalThis.gBrowser.selectedBrowser as unknown as HTMLElement).focus();
      }
      event.preventDefault();
    }
  };
}
