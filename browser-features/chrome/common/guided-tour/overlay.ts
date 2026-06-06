/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TooltipPlacement, TourStep } from "./types.ts";

const HTML_NS = "http://www.w3.org/1999/xhtml";
const OVERLAY_ID = "floorp-guided-tour-overlay";
const STYLES_ID = "floorp-guided-tour-styles";
const SPOTLIGHT_PADDING = 6;

interface TourCallbacks {
  onNext: () => void;
  onPrev: () => void;
  onSkip: () => void;
}

function injectStyles(doc: Document): void {
  const existing = doc.getElementById(STYLES_ID);
  if (existing) {
    existing.remove();
  }

  const style = doc.createElement("style");
  style.id = STYLES_ID;
  style.textContent = `
    #${OVERLAY_ID} {
      position: fixed;
      inset: 0;
      z-index: 999999;
      pointer-events: none;
    }
    #${OVERLAY_ID}.floorp-tour-passthrough {
      pointer-events: none;
    }
    .floorp-tour-backdrop {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.45);
      z-index: 1;
      pointer-events: auto;
      transition: opacity 0.25s ease;
    }
    .floorp-tour-passthrough .floorp-tour-backdrop {
      pointer-events: none;
    }
    .floorp-tour-spotlight {
      position: fixed;
      z-index: 2;
      border-radius: 6px;
      border: 3px solid rgba(99, 179, 237, 0.95);
      background: rgba(99, 179, 237, 0.12);
      box-shadow: 0 0 0 9999px rgba(0, 0, 0, 0.5), 0 0 16px 6px rgba(99, 179, 237, 0.4);
      pointer-events: none;
      transition: all 0.3s ease;
    }
    .floorp-tour-tooltip {
      position: fixed;
      z-index: 3;
      background: -moz-dialog;
      color: -moz-dialogtext;
      border: 1px solid rgba(0, 0, 0, 0.15);
      border-radius: 8px;
      box-shadow: 0 4px 24px rgba(0, 0, 0, 0.18);
      max-width: 360px;
      min-width: 260px;
      padding: 16px;
      pointer-events: auto;
      opacity: 0;
      transform: translateY(6px);
      transition: opacity 0.2s ease, transform 0.2s ease;
      font: message-box;
    }
    .floorp-tour-tooltip.floorp-tour-visible {
      opacity: 1;
      transform: translateY(0);
    }
    .floorp-tour-tooltip-title {
      font-weight: bold;
      font-size: 14px;
      margin-bottom: 8px;
    }
    .floorp-tour-tooltip-desc {
      font-size: 13px;
      line-height: 1.5;
      margin-bottom: 14px;
      opacity: 0.85;
    }
    .floorp-tour-tooltip-nav {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }
    .floorp-tour-btn {
      appearance: none;
      border: 1px solid rgba(0, 0, 0, 0.15);
      border-radius: 4px;
      background: -moz-dialog;
      color: -moz-dialogtext;
      padding: 4px 12px;
      font: message-box;
      font-size: 12px;
      cursor: pointer;
    }
    .floorp-tour-btn:hover {
      background: rgba(0, 0, 0, 0.06);
    }
    .floorp-tour-btn-primary {
      background: AccentColor;
      color: AccentColorText;
      border-color: transparent;
    }
    .floorp-tour-btn-primary:hover {
      filter: brightness(1.1);
    }
    .floorp-tour-skip {
      appearance: none;
      border: none;
      background: transparent;
      color: -moz-dialogtext;
      opacity: 0.5;
      cursor: pointer;
      font: message-box;
      font-size: 11px;
      padding: 2px 4px;
    }
    .floorp-tour-skip:hover {
      opacity: 0.8;
      text-decoration: underline;
    }
    .floorp-tour-dots {
      display: flex;
      gap: 6px;
      align-items: center;
    }
    .floorp-tour-dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: rgba(0, 0, 0, 0.2);
      transition: background 0.2s ease;
    }
    .floorp-tour-dot-active {
      background: AccentColor;
      width: 8px;
      height: 8px;
    }
    .floorp-tour-center .floorp-tour-tooltip {
      left: 50%;
      top: 50%;
      transform: translate(-50%, -50%) translateY(6px);
    }
    .floorp-tour-center .floorp-tour-tooltip.floorp-tour-visible {
      transform: translate(-50%, -50%) translateY(0);
    }
  `;
  doc.head!.appendChild(style);
}

export class GuidedTourOverlay {
  private root: HTMLDivElement | null = null;
  private spotlightEl: HTMLDivElement | null = null;
  private tooltipEl: HTMLDivElement | null = null;
  private backdropEl: HTMLDivElement | null = null;
  private tooltipTitleEl: HTMLDivElement | null = null;
  private tooltipDescEl: HTMLDivElement | null = null;
  private dotsContainer: HTMLDivElement | null = null;
  private resizeHandler: (() => void) | null = null;
  private targetWindow: Window;
  private callbacks: TourCallbacks;
  private stepCount: number = 0;
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

    this.root = doc.createElement("div") as HTMLDivElement;
    this.root.id = OVERLAY_ID;

    this.backdropEl = doc.createElement("div") as HTMLDivElement;
    this.backdropEl.className = "floorp-tour-backdrop";
    this.backdropEl.addEventListener("click", () => this.callbacks.onSkip());

    this.spotlightEl = doc.createElement("div") as HTMLDivElement;
    this.spotlightEl.className = "floorp-tour-spotlight";
    this.spotlightEl.style.display = "none";

    this.tooltipEl = doc.createElement("div") as HTMLDivElement;
    this.tooltipEl.className = "floorp-tour-tooltip";

    this.tooltipTitleEl = doc.createElement("div") as HTMLDivElement;
    this.tooltipTitleEl.className = "floorp-tour-tooltip-title";

    const tooltipDesc = doc.createElement("div") as HTMLDivElement;
    tooltipDesc.className = "floorp-tour-tooltip-desc";
    this.tooltipDescEl = tooltipDesc;

    const nav = doc.createElement("div") as HTMLDivElement;
    nav.className = "floorp-tour-tooltip-nav";

    const skipBtn = doc.createElement("button") as HTMLButtonElement;
    skipBtn.className = "floorp-tour-skip";
    skipBtn.textContent = "Skip";
    skipBtn.addEventListener("click", () => this.callbacks.onSkip());

    const dots = doc.createElement("div") as HTMLDivElement;
    dots.className = "floorp-tour-dots";
    this.dotsContainer = dots;

    const prevBtn = doc.createElement("button") as HTMLButtonElement;
    prevBtn.className = "floorp-tour-btn";
    prevBtn.textContent = "← Prev";
    prevBtn.addEventListener("click", () => this.callbacks.onPrev());

    const nextBtn = doc.createElement("button") as HTMLButtonElement;
    nextBtn.className = "floorp-tour-btn floorp-tour-btn-primary";
    nextBtn.textContent = "Next →";
    nextBtn.addEventListener("click", () => this.callbacks.onNext());

    nav.appendChild(skipBtn);
    nav.appendChild(dots);
    nav.appendChild(prevBtn);
    nav.appendChild(nextBtn);

    this.tooltipEl.appendChild(this.tooltipTitleEl);
    this.tooltipEl.appendChild(tooltipDesc);
    this.tooltipEl.appendChild(nav);

    this.root.appendChild(this.backdropEl);
    this.root.appendChild(this.spotlightEl);
    this.root.appendChild(this.tooltipEl);

    doc.documentElement!.appendChild(this.root);

    this.resizeHandler = () => this.repositionTooltip();
    this.targetWindow.addEventListener("resize", this.resizeHandler);

    // show animation
    requestAnimationFrame(() => {
      if (this.tooltipEl) this.tooltipEl.classList.add("floorp-tour-visible");
    });
  }

  updateStep(step: TourStep, currentStep: number, totalSteps: number): void {
    this.stepCount = totalSteps;
    if (!this.root || !this.tooltipTitleEl || !this.tooltipDescEl) return;

    // Update passthrough mode
    if (step.passthrough) {
      this.root.classList.add("floorp-tour-passthrough");
    } else {
      this.root.classList.remove("floorp-tour-passthrough");
    }

    // Update text
    this.tooltipTitleEl.textContent = step.titleKey;
    this.tooltipDescEl.textContent = step.descriptionKey;

    // Update dots
    this.updateDots(currentStep, totalSteps);

    // Update button labels
    const nav = this.tooltipEl!.querySelector(".floorp-tour-tooltip-nav");
    if (nav) {
      const skipBtn = nav.querySelector(".floorp-tour-skip") as HTMLButtonElement;
      const prevBtn = nav.querySelector(".floorp-tour-btn:not(.floorp-tour-btn-primary)") as HTMLButtonElement;
      const nextBtn = nav.querySelector(".floorp-tour-btn-primary") as HTMLButtonElement;

      if (skipBtn) skipBtn.textContent = currentStep === totalSteps - 1 ? "" : "Skip";
      if (prevBtn) prevBtn.style.visibility = currentStep === 0 ? "hidden" : "visible";
      if (nextBtn) nextBtn.textContent = currentStep === totalSteps - 1 ? "Got it" : "Next →";
    }

    // Position spotlight and tooltip
    this.currentSelector = step.selector;
    this.currentPlacement = step.tooltipPlacement;
    this.positionSpotlight(step.selector, step.tooltipPlacement);

    // Handle center placement class
    if (step.tooltipPlacement === "center" || !step.selector) {
      this.root.classList.add("floorp-tour-center");
    } else {
      this.root.classList.remove("floorp-tour-center");
    }

    // Reset tooltip visibility for animation
    this.tooltipEl!.classList.remove("floorp-tour-visible");
    requestAnimationFrame(() => {
      if (this.tooltipEl) this.tooltipEl.classList.add("floorp-tour-visible");
    });
  }

  private updateDots(currentStep: number, totalSteps: number): void {
    if (!this.dotsContainer) return;
    const doc = this.targetWindow.document;
    if (!doc) return;
    this.dotsContainer.innerHTML = "";
    for (let i = 0; i < totalSteps; i++) {
      const dot = doc.createElement("div") as HTMLDivElement;
      dot.className = i === currentStep
        ? "floorp-tour-dot floorp-tour-dot-active"
        : "floorp-tour-dot";
      this.dotsContainer.appendChild(dot);
    }
  }

  private positionSpotlight(
    selector: string | null,
    placement: TooltipPlacement,
  ): void {
    if (!this.spotlightEl || !this.tooltipEl) return;

    if (!selector) {
      // Full-screen centered mode
      this.spotlightEl.style.display = "none";
      this.tooltipEl.style.left = "50%";
      this.tooltipEl.style.top = "50%";
      this.tooltipEl.style.transform = "translate(-50%, -50%)";
      return;
    }

    const target = this.targetWindow.document?.querySelector(selector);
    if (!target) {
      this.spotlightEl.style.display = "none";
      // Fallback to center
      this.tooltipEl.style.left = "50%";
      this.tooltipEl.style.top = "50%";
      this.tooltipEl.style.transform = "translate(-50%, -50%)";
      return;
    }

    const rect = target.getBoundingClientRect();
    const pad = SPOTLIGHT_PADDING;

    // Show spotlight
    this.spotlightEl.style.display = "block";
    this.spotlightEl.style.left = `${rect.left - pad}px`;
    this.spotlightEl.style.top = `${rect.top - pad}px`;
    this.spotlightEl.style.width = `${rect.width + pad * 2}px`;
    this.spotlightEl.style.height = `${rect.height + pad * 2}px`;

    // Position tooltip
    this.positionTooltip(rect, placement);
  }

  private positionTooltip(
    targetRect: DOMRect,
    placement: TooltipPlacement,
  ): void {
    if (!this.tooltipEl) return;

    const tooltipWidth = 340;
    const tooltipHeight = 160;
    const gap = 12;
    const vw = this.targetWindow.innerWidth;
    const vh = this.targetWindow.innerHeight;

    let left: number;
    let top: number;
    let transform: string = "";

    switch (placement) {
      case "bottom":
        left = targetRect.left + targetRect.width / 2;
        top = targetRect.bottom + gap;
        transform = "translateX(-50%)";
        break;
      case "top":
        left = targetRect.left + targetRect.width / 2;
        top = targetRect.top - tooltipHeight - gap;
        transform = "translateX(-50%)";
        break;
      case "right":
        left = targetRect.right + gap;
        top = targetRect.top + targetRect.height / 2;
        transform = "translateY(-50%)";
        break;
      case "left":
        left = targetRect.left - tooltipWidth - gap;
        top = targetRect.top + targetRect.height / 2;
        transform = "translateY(-50%)";
        break;
      case "center":
      default:
        left = vw / 2;
        top = vh / 2;
        transform = "translate(-50%, -50%)";
        break;
    }

    // Clamp to viewport
    left = Math.max(gap, Math.min(left, vw - tooltipWidth - gap));
    top = Math.max(gap, Math.min(top, vh - tooltipHeight - gap));

    this.tooltipEl.style.left = `${left}px`;
    this.tooltipEl.style.top = `${top}px`;
    this.tooltipEl.style.transform = transform;
  }

  private repositionTooltip(): void {
    // Recalculate both spotlight and tooltip positions on resize
    if (this.currentSelector) {
      this.positionSpotlight(this.currentSelector, this.currentPlacement);
    }
  }

  destroy(): void {
    if (this.resizeHandler) {
      this.targetWindow.removeEventListener("resize", this.resizeHandler);
      this.resizeHandler = null;
    }

    if (this.root && this.root.parentNode) {
      this.root.parentNode.removeChild(this.root);
    }

    this.root = null;
    this.spotlightEl = null;
    this.tooltipEl = null;
    this.backdropEl = null;
    this.tooltipTitleEl = null;
    this.tooltipDescEl = null;
    this.dotsContainer = null;
  }
}
