/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import type { TooltipPlacement, TourStep } from "./types.ts";

const PREFIX = "floorp-guided-tour";
const STYLES_ID = `${PREFIX}-styles`;

// XUL panel helper type — matches UITour.sys.mjs usage
interface XulPanel extends XULElement {
  openPopup(
    anchor: Element | null,
    position: string,
    x?: number,
    y?: number,
  ): void;
  moveTo(x: number, y: number): void;
  hidePopup(): void;
  readonly state: string;
}

function createPanel(
  doc: Document,
  id: string,
  attrs: Record<string, string>,
): XulPanel | null {
  const popupSet = doc.getElementById("mainPopupSet");
  if (!popupSet || !doc.createXULElement) return null;
  const panel = doc.createXULElement("panel") as unknown as XulPanel;
  panel.id = id;
  for (const [k, v] of Object.entries(attrs)) panel.setAttribute(k, v);
  popupSet.appendChild(panel);
  return panel;
}

function closePanel(panel: XulPanel | null): void {
  if (!panel) return;
  try {
    panel.hidePopup();
  } catch {
    /* already closed */
  }
  panel.remove();
}

function isOpen(panel: XulPanel | null): boolean {
  return !!panel && (panel.state === "showing" || panel.state === "open");
}

interface TourCallbacks {
  onNext: () => void;
  onPrev: () => void;
  onSkip: () => void;
}

function injectStyles(doc: Document): void {
  const existing = doc.getElementById(STYLES_ID);
  if (existing) existing.remove();
  const style = doc.createElement("style");
  style.id = STYLES_ID;
  style.textContent = `
    #${PREFIX}-spotlight-panel {
      appearance: none;
      pointer-events: none;
      border: none;
      background-color: transparent;
      -moz-window-shadow: none;
      overflow: visible;
      --panel-border-color: transparent;
      --panel-background: transparent;
      --panel-padding: 0;
    }
    #${PREFIX}-spotlight {
      border-radius: 6px;
      border: 3px solid rgba(99, 179, 237, 0.95);
      background: rgba(99, 179, 237, 0.12);
      box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.35), 0 0 16px 6px rgba(99, 179, 237, 0.4);
      min-height: 24px; min-width: 24px;
      transition: all 0.3s ease;
    }
    #${PREFIX}-tooltip-panel {
      appearance: none;
      background: transparent; border: none;
      -moz-window-shadow: none;
      --panel-border-color: transparent;
      --panel-background: transparent;
      --panel-padding: 0;
    }
    #${PREFIX}-tooltip {
      background: -moz-dialog; color: -moz-dialogtext;
      border: 1px solid rgba(0,0,0,0.15); border-radius: 8px;
      box-shadow: 0 4px 24px rgba(0,0,0,0.18);
      max-width: 360px; min-width: 260px;
      padding: 16px; font: message-box; pointer-events: auto;
    }
    .floorp-tour-tooltip-title { font-weight: bold; font-size: 14px; margin-bottom: 8px; }
    .floorp-tour-tooltip-desc  { font-size: 13px; line-height: 1.5; margin-bottom: 14px; opacity: 0.85; }
    .floorp-tour-tooltip-nav   { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
    .floorp-tour-btn {
      appearance: none; border: 1px solid rgba(0,0,0,0.15); border-radius: 4px;
      background: -moz-dialog; color: -moz-dialogtext;
      padding: 4px 12px; font: message-box; font-size: 12px; cursor: pointer;
    }
    .floorp-tour-btn:hover       { background: rgba(0,0,0,0.06); }
    .floorp-tour-btn-primary     { background: AccentColor; color: AccentColorText; border-color: transparent; }
    .floorp-tour-btn-primary:hover { filter: brightness(1.1); }
    .floorp-tour-skip {
      appearance: none; border: none; background: transparent;
      color: -moz-dialogtext; opacity: 0.5; cursor: pointer;
      font: message-box; font-size: 11px; padding: 2px 4px;
    }
    .floorp-tour-skip:hover { opacity: 0.8; text-decoration: underline; }
    .floorp-tour-dots      { display: flex; gap: 6px; align-items: center; }
    .floorp-tour-dot       { width: 6px; height: 6px; border-radius: 50%; background: rgba(0,0,0,0.2); transition: background 0.2s ease; }
    .floorp-tour-dot-active { background: AccentColor; width: 8px; height: 8px; }
  `;
  doc.head!.appendChild(style);
}

export class GuidedTourOverlay {
  private spotlightPanel: XulPanel | null = null;
  private spotlightBox: HTMLDivElement | null = null;
  private tooltipPanel: XulPanel | null = null;
  private tooltipEl: HTMLDivElement | null = null;
  private tooltipTitleEl: HTMLDivElement | null = null;
  private tooltipDescEl: HTMLDivElement | null = null;
  private dotsContainer: HTMLDivElement | null = null;
  private resizeHandler: (() => void) | null = null;
  private targetWindow: Window;
  private callbacks: TourCallbacks;
  private currentSelector: string | null = null;
  private currentPlacement: TooltipPlacement = "bottom";

  constructor(win: Window, callbacks: TourCallbacks) {
    this.targetWindow = win;
    this.callbacks = callbacks;
  }

  create(): void {
    const doc = this.targetWindow.document;
    if (!doc) return;
    injectStyles(doc);

    // Spotlight panel (UITour highlight pattern)
    this.spotlightPanel = createPanel(doc, `${PREFIX}-spotlight-panel`, {
      level: "top",
      type: "default",
      noautohide: "true",
      noautofocus: "true",
      flip: "none",
      consumeoutsideclicks: "false",
    });
    this.spotlightBox = doc.createElement("div") as HTMLDivElement;
    this.spotlightBox.id = `${PREFIX}-spotlight`;
    this.spotlightBox.style.display = "none";
    this.spotlightPanel?.appendChild(this.spotlightBox);

    // Tooltip panel (UITour tooltip pattern)
    this.tooltipPanel = createPanel(doc, `${PREFIX}-tooltip-panel`, {
      level: "top",
      noautohide: "true",
      noautofocus: "true",
      // Allow the tooltip to flip to stay within the viewport
      flip: "both",
    });
    this.tooltipEl = doc.createElement("div") as HTMLDivElement;
    this.tooltipEl.id = `${PREFIX}-tooltip`;
    this.tooltipTitleEl = doc.createElement("div") as HTMLDivElement;
    this.tooltipTitleEl.className = "floorp-tour-tooltip-title";
    this.tooltipDescEl = doc.createElement("div") as HTMLDivElement;
    this.tooltipDescEl.className = "floorp-tour-tooltip-desc";

    const nav = doc.createElement("div") as HTMLDivElement;
    nav.className = "floorp-tour-tooltip-nav";

    const skipBtn = doc.createElement("button") as HTMLButtonElement;
    skipBtn.className = "floorp-tour-skip";
    skipBtn.textContent = i18next.t("guidedTour.skip");
    skipBtn.addEventListener("click", () => this.callbacks.onSkip());

    this.dotsContainer = doc.createElement("div") as HTMLDivElement;
    this.dotsContainer.className = "floorp-tour-dots";

    const prevBtn = doc.createElement("button") as HTMLButtonElement;
    prevBtn.className = "floorp-tour-btn";
    prevBtn.textContent = i18next.t("guidedTour.previous");
    prevBtn.addEventListener("click", () => this.callbacks.onPrev());

    const nextBtn = doc.createElement("button") as HTMLButtonElement;
    nextBtn.className = "floorp-tour-btn floorp-tour-btn-primary";
    nextBtn.textContent = i18next.t("guidedTour.next");
    nextBtn.addEventListener("click", () => this.callbacks.onNext());

    nav.appendChild(skipBtn);
    nav.appendChild(this.dotsContainer);
    nav.appendChild(prevBtn);
    nav.appendChild(nextBtn);

    this.tooltipEl.appendChild(this.tooltipTitleEl);
    this.tooltipEl.appendChild(this.tooltipDescEl);
    this.tooltipEl.appendChild(nav);
    this.tooltipPanel?.appendChild(this.tooltipEl);

    this.resizeHandler = () => this.reposition();
    this.targetWindow.addEventListener("resize", this.resizeHandler);
  }

  updateStep(step: TourStep, currentStep: number, totalSteps: number): void {
    if (!this.tooltipTitleEl || !this.tooltipDescEl) return;

    console.log("[GuidedTour:updateStep]", {
      currentStep,
      totalSteps,
      selector: step.selector,
      placement: step.tooltipPlacement,
      titleKey: step.titleKey,
    });

    this.tooltipTitleEl.textContent = step.titleKey;
    this.tooltipDescEl.textContent = step.descriptionKey;
    this.updateDots(currentStep, totalSteps);

    const nav = this.tooltipEl?.querySelector(".floorp-tour-tooltip-nav");
    if (nav) {
      const skip = nav.querySelector(".floorp-tour-skip") as HTMLButtonElement;
      const prev = nav.querySelector(
        ".floorp-tour-btn:not(.floorp-tour-btn-primary)",
      ) as HTMLButtonElement;
      const next = nav.querySelector(
        ".floorp-tour-btn-primary",
      ) as HTMLButtonElement;
      if (skip)
        skip.textContent =
          currentStep === totalSteps - 1 ? "" : i18next.t("guidedTour.skip");
      if (prev)
        prev.style.visibility = currentStep === 0 ? "hidden" : "visible";
      if (next)
        next.textContent =
          currentStep === totalSteps - 1
            ? i18next.t("guidedTour.finish")
            : i18next.t("guidedTour.next");
    }

    this.currentSelector = step.selector;
    this.currentPlacement = step.tooltipPlacement;
    this.positionPanels(step.selector, step.tooltipPlacement);
  }

  private positionPanels(
    selector: string | null,
    placement: TooltipPlacement,
  ): void {
    const doc = this.targetWindow.document;
    if (!doc) return;

    // Debug: log positioning info
    console.log("[GuidedTour:positionPanels]", {
      selector,
      placement,
      spotlightPanel: !!this.spotlightPanel,
      tooltipPanel: !!this.tooltipPanel,
    });

    // Spotlight — UITour: openPopup(target, "overlap", offsetX, offsetY)
    if (selector) {
      const target = doc.querySelector(selector);
      if (target && this.spotlightPanel && this.spotlightBox) {
        const rect = target.getBoundingClientRect();
        const minSize = 24;
        const highlightW = Math.max(rect.width, minSize);
        const highlightH = Math.max(rect.height, minSize);
        this.spotlightBox.style.width = `${highlightW}px`;
        this.spotlightBox.style.height = `${highlightH}px`;
        this.spotlightBox.style.display = "block";
        const offsetX = (rect.width - highlightW) / 2;
        const offsetY = (rect.height - highlightH) / 2;
        // MovePopupTo avoids hidePopup which can cascade-close other XUL popups
        if (isOpen(this.spotlightPanel)) {
          try {
            this.spotlightPanel.moveTo(
              Math.round(rect.x + offsetX),
              Math.round(rect.y + offsetY),
            );
          } catch {
            // fallback: hide and reopen
            this.spotlightPanel.hidePopup();
            this.spotlightPanel.openPopup(target, "overlap", offsetX, offsetY);
          }
        } else {
          this.spotlightPanel.openPopup(target, "overlap", offsetX, offsetY);
        }
        const spotRect = this.spotlightBox.getBoundingClientRect();
        console.log("[GuidedTour:spotlight:positioned]", {
          target: { tag: target.tagName, id: target.id, x: rect.x, y: rect.y, w: rect.width, h: rect.height },
          spotRect: { x: spotRect.x, y: spotRect.y, w: spotRect.width, h: spotRect.height },
        });
      } else {
        console.log("[GuidedTour:spotlight] target not found or panel missing", {
          target: !!target,
          spotlightPanel: !!this.spotlightPanel,
          spotlightBox: !!this.spotlightBox,
        });
        this.spotlightBox?.style.setProperty("display", "none");
        if (isOpen(this.spotlightPanel)) this.spotlightPanel!.hidePopup();
      }
    } else {
      this.spotlightBox?.style.setProperty("display", "none");
      if (isOpen(this.spotlightPanel)) this.spotlightPanel!.hidePopup();
    }

    // Tooltip — UITour: openPopup(target, alignment, x, y)
    if (isOpen(this.tooltipPanel)) this.tooltipPanel!.hidePopup();
    if (selector) {
      const target = doc.querySelector(selector);
      if (target && this.tooltipPanel) {
        const alignment = this.placementToAlignment(placement);
        const targetRect = target.getBoundingClientRect();
        this.tooltipPanel.openPopup(target, alignment, 0, 0);
        // Log after popup is positioned
        const tooltipRect = this.tooltipEl?.getBoundingClientRect();
        console.log("[GuidedTour:tooltip:positioned]", {
          viewport: { w: this.targetWindow.innerWidth, h: this.targetWindow.innerHeight },
          target: { tag: target.tagName, id: target.id, x: targetRect.x, y: targetRect.y, w: targetRect.width, h: targetRect.height },
          alignment,
          tooltipPanelState: this.tooltipPanel.state,
          tooltipRect: tooltipRect ? { x: tooltipRect.x, y: tooltipRect.y, w: tooltipRect.width, h: tooltipRect.height } : null,
        });
      } else {
        console.log("[GuidedTour:tooltip] target not found", {
          target: !!target,
          tooltipPanel: !!this.tooltipPanel,
        });
      }
    } else if (this.tooltipPanel) {
      const browser = doc.getElementById("browser");
      this.tooltipPanel.openPopup(browser, "after_start", 0, 0);
      const tooltipRect = this.tooltipEl?.getBoundingClientRect();
      console.log("[GuidedTour:tooltip:positioned] center mode", {
        viewport: { w: this.targetWindow.innerWidth, h: this.targetWindow.innerHeight },
        tooltipRect: tooltipRect ? { x: tooltipRect.x, y: tooltipRect.y, w: tooltipRect.width, h: tooltipRect.height } : null,
      });
    }
  }

  private placementToAlignment(placement: TooltipPlacement): string {
    switch (placement) {
      case "bottom":
        return "bottomleft topleft";
      case "top":
        return "topleft bottomleft";
      case "right":
        return "start_before";
      case "left":
        return "end_before";
      default:
        return "overlap";
    }
  }

  private updateDots(currentStep: number, totalSteps: number): void {
    if (!this.dotsContainer) return;
    const doc = this.targetWindow.document;
    if (!doc) return;
    this.dotsContainer.innerHTML = "";
    for (let i = 0; i < totalSteps; i++) {
      const dot = doc.createElement("div") as HTMLDivElement;
      dot.className =
        i === currentStep
          ? "floorp-tour-dot floorp-tour-dot-active"
          : "floorp-tour-dot";
      this.dotsContainer.appendChild(dot);
    }
  }

  private reposition(): void {
    console.log("[GuidedTour:reposition]", {
      spotlightState: this.spotlightPanel?.state,
      tooltipState: this.tooltipPanel?.state,
      selector: this.currentSelector,
    });
    if (this.currentSelector) {
      this.positionPanels(this.currentSelector, this.currentPlacement);
    }
  }

  private lastStateHash = "";

  /** Check if panels are still open and log state (only on change) */
  checkPanelState(): void {
    const spotlightOpen = isOpen(this.spotlightPanel);
    const tooltipOpen = isOpen(this.tooltipPanel);
    const tooltipRect = this.tooltipEl?.getBoundingClientRect();
    const wsPanel = this.targetWindow.document?.getElementById("workspacesToolbarButtonPanel");
    const wsPanelRect = wsPanel?.getBoundingClientRect();
    const wsPanelOpen = wsPanelRect ? (wsPanelRect.width > 0 && wsPanelRect.height > 0) : false;

    const stateHash = `${spotlightOpen}:${tooltipOpen}:${wsPanelOpen}`;
    if (stateHash === this.lastStateHash) return; // skip unchanged
    this.lastStateHash = stateHash;

    console.log("[GuidedTour:panelState]", {
      spotlightOpen,
      tooltipOpen,
      tooltipRect: tooltipRect ? { x: tooltipRect.x, y: tooltipRect.y, w: tooltipRect.width, h: tooltipRect.height } : null,
      wsPanelOpen,
      wsPanelRect: wsPanelRect ? { x: wsPanelRect.x, y: wsPanelRect.y, w: wsPanelRect.width, h: wsPanelRect.height } : null,
    });
    // Auto-recover: if tooltip closed but should be open, re-position
    if (!tooltipOpen && this.currentSelector) {
      console.log("[GuidedTour:panelState] tooltip disappeared, re-positioning");
      this.positionPanels(this.currentSelector, this.currentPlacement);
    }
  }

  destroy(): void {
    if (this.resizeHandler) {
      this.targetWindow.removeEventListener("resize", this.resizeHandler);
      this.resizeHandler = null;
    }
    closePanel(this.spotlightPanel);
    closePanel(this.tooltipPanel);
    this.spotlightPanel = null;
    this.spotlightBox = null;
    this.tooltipPanel = null;
    this.tooltipEl = null;
    this.tooltipTitleEl = null;
    this.tooltipDescEl = null;
    this.dotsContainer = null;
  }
}
