/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * HighlightManager - Manages highlight overlays and info panels for web scraping visualization
 */

import {
  HIGHLIGHT_PRESETS,
  HIGHLIGHT_STYLES,
  LUCIDE_ICON_PATHS,
  MINIMUM_HIGHLIGHT_DURATION,
} from "./constants.ts";
import type {
  HighlightOptionsInput,
  NormalizedHighlightOptions,
  WebScraperContext,
} from "./types.ts";
import { TranslationHelper } from "./TranslationHelper.ts";

const { setTimeout: timerSetTimeout, clearTimeout: timerClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

/**
 * Manages highlight overlays, info panels, and control overlays
 */
export class HighlightManager {
  private highlightOverlay: HTMLDivElement | null = null;
  private highlightOverlays: HTMLDivElement[] = [];
  private highlightCleanupTimer: number | null = null;
  private highlightCleanupCallbacks: Array<() => void> = [];
  private highlightStyleElement: HTMLStyleElement | null = null;
  private infoPanel: HTMLDivElement | null = null;
  private infoPanelCleanupTimer: number | null = null;
  private controlOverlay: HTMLDivElement | null = null;
  private controlOverlayLabel: HTMLDivElement | null = null;

  // Control overlay event handlers for cleanup
  private controlOverlayHandlers: {
    blockEvent: (e: Event) => void;
    blockKeydown: (e: KeyboardEvent) => void;
    blockFocus: (e: FocusEvent) => void;
  } | null = null;

  // 非同期操作の競合防止用 操作ID
  private highlightOperationId = 0;
  // ハイライトの最小表示時間を保証するためのタイムスタンプ
  private highlightStartTime = 0;

  private translationHelper: TranslationHelper;

  constructor(private context: WebScraperContext) {
    this.translationHelper = new TranslationHelper(context);
  }

  get document(): Document | null {
    return this.context.document;
  }

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  /**
   * Get current operation ID for cancellation tracking
   */
  getOperationId(): number {
    return this.highlightOperationId;
  }

  /**
   * Ensures the highlight style element is injected into the document
   */
  ensureHighlightStyle(): void {
    const doc = this.document;
    if (!doc) {
      return;
    }

    if (this.highlightStyleElement?.isConnected) {
      return;
    }

    const existing = doc.getElementById("nr-webscraper-highlight-style");
    if (existing instanceof HTMLStyleElement) {
      this.highlightStyleElement = existing;
      return;
    }

    const style = doc.createElement("style");
    style.id = "nr-webscraper-highlight-style";
    style.textContent = HIGHLIGHT_STYLES;

    (doc.head ?? doc.documentElement)?.append(style);
    this.highlightStyleElement = style;
  }

  /**
   * Get highlight options for an action
   */
  getHighlightOptions(
    action: string,
    overrides: HighlightOptionsInput = {},
  ): HighlightOptionsInput {
    const base = HIGHLIGHT_PRESETS[action] ?? HIGHLIGHT_PRESETS.Highlight;
    return { ...base, ...overrides, action };
  }

  /**
   * Normalize highlight options with defaults
   */
  normalizeHighlightOptions(
    options: HighlightOptionsInput = {},
  ): NormalizedHighlightOptions {
    const action = options.action ?? "Highlight";
    return {
      action,
      duration: Math.max(options.duration ?? 1800, 300),
      focus: options.focus ?? true,
      scrollBehavior: options.scrollBehavior ?? "smooth",
      padding: Math.max(options.padding ?? 14, 0),
      delay: Math.max(options.delay ?? 400, 0),
    };
  }

  /**
   * Get action color class based on action type
   */
  getActionColorClass(action?: string): string {
    if (!action) return "";
    const lowerAction = action.toLowerCase();
    if (
      lowerAction.includes("read") ||
      lowerAction.includes("inspect") ||
      lowerAction.includes("get")
    ) {
      return "nr-webscraper-highlight-overlay--read";
    }
    if (
      lowerAction.includes("input") ||
      lowerAction.includes("fill") ||
      lowerAction.includes("write")
    ) {
      return "nr-webscraper-highlight-overlay--write";
    }
    if (lowerAction.includes("click")) {
      return "nr-webscraper-highlight-overlay--click";
    }
    if (lowerAction.includes("submit")) {
      return "nr-webscraper-highlight-overlay--submit";
    }
    return "";
  }

  /**
   * Creates an SVG icon element using Lucide icon paths
   */
  createLucideIcon(
    doc: Document,
    iconName: keyof typeof LUCIDE_ICON_PATHS,
    size = 20,
  ): Element {
    const svgString = `<svg width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" xmlns="http://www.w3.org/2000/svg"><path d="${LUCIDE_ICON_PATHS[iconName] || LUCIDE_ICON_PATHS.Zap}"/></svg>`;
    const div = doc.createElement("div");
    div.innerHTML = svgString;
    const svg = div.firstElementChild;
    if (!svg) {
      // Fallback: create a simple SVG element
      const fallbackSvg = doc.createElementNS(
        "http://www.w3.org/2000/svg",
        "svg",
      );
      fallbackSvg.setAttribute("width", String(size));
      fallbackSvg.setAttribute("height", String(size));
      fallbackSvg.setAttribute("viewBox", "0 0 24 24");
      return fallbackSvg;
    }
    return svg;
  }

  /**
   * Get action icon (SVG element or emoji fallback)
   */
  getActionIcon(action: string): Element | string {
    const doc = this.document;
    if (!doc) {
      return this.getActionIconEmoji(action);
    }

    const lowerAction = action.toLowerCase();
    if (
      lowerAction.includes("read") ||
      lowerAction.includes("inspect") ||
      lowerAction.includes("get")
    ) {
      return this.createLucideIcon(doc, "Eye", 20);
    }
    if (
      lowerAction.includes("input") ||
      lowerAction.includes("fill") ||
      lowerAction.includes("write")
    ) {
      return this.createLucideIcon(doc, "Pencil", 20);
    }
    if (lowerAction.includes("click")) {
      return this.createLucideIcon(doc, "MousePointerClick", 20);
    }
    if (lowerAction.includes("submit")) {
      return this.createLucideIcon(doc, "Send", 20);
    }
    return this.createLucideIcon(doc, "Zap", 20);
  }

  /**
   * Get action icon as emoji
   */
  getActionIconEmoji(action: string): string {
    const lowerAction = action.toLowerCase();
    if (
      lowerAction.includes("read") ||
      lowerAction.includes("inspect") ||
      lowerAction.includes("get")
    ) {
      return "👁️";
    }
    if (
      lowerAction.includes("input") ||
      lowerAction.includes("fill") ||
      lowerAction.includes("write")
    ) {
      return "✏️";
    }
    if (lowerAction.includes("click")) {
      return "👆";
    }
    if (lowerAction.includes("submit")) {
      return "📤";
    }
    return "⚡";
  }

  /**
   * Show info panel with action details
   */
  async showInfoPanel(
    action: string,
    selector?: string,
    elementInfo?: string,
    elementCount?: number,
    currentProgress?: number,
    totalProgress?: number,
  ): Promise<void> {
    try {
      const win = this.contentWindow;
      const doc = this.document;
      if (!win || !doc) return;

      this.hideInfoPanel();
      this.ensureHighlightStyle();

      const panel = doc.createElement("div");
      panel.className = "nr-webscraper-info-panel";

      const colorClass = this.getActionColorClass(action);
      if (colorClass) {
        panel.classList.add(colorClass);
      }

      const title = doc.createElement("div");
      title.className = "nr-webscraper-info-panel__title";

      const icon = doc.createElement("span");
      icon.className = "nr-webscraper-info-panel__icon";
      const iconElement = this.getActionIcon(action);
      if (typeof iconElement === "string") {
        icon.textContent = iconElement;
      } else if (iconElement && iconElement.nodeName === "svg") {
        icon.appendChild(iconElement);
      } else {
        icon.textContent = this.getActionIconEmoji(action);
      }

      const badge = doc.createElement("span");
      badge.className = "nr-webscraper-info-panel__badge";
      badge.textContent = await this.translationHelper.getActionLabel(action);

      title.append(icon);
      title.append(badge);
      title.append(
        doc.createTextNode(
          await this.translationHelper.translate("panelTitle"),
        ),
      );

      const content = doc.createElement("div");
      content.className = "nr-webscraper-info-panel__content";

      if (elementInfo) {
        content.textContent = elementInfo;
      } else {
        content.textContent = await this.translationHelper.translate(
          "panelDefaultMessage",
          {
            action: await this.translationHelper.getActionLabel(action),
          },
        );
      }

      panel.append(title);
      panel.append(content);

      // 進捗バーを追加（複数要素操作時）
      if (
        totalProgress !== undefined &&
        totalProgress > 1 &&
        currentProgress !== undefined
      ) {
        const progressContainer = doc.createElement("div");
        progressContainer.className = "nr-webscraper-info-panel__progress";
        const progressBar = doc.createElement("div");
        progressBar.className = "nr-webscraper-info-panel__progress-bar";
        const progressPercent = Math.min(
          100,
          Math.round((currentProgress / totalProgress) * 100),
        );
        progressBar.style.setProperty("animation", "none");
        progressBar.style.setProperty("width", "0%");
        progressContainer.append(progressBar);
        const animateProgress = () => {
          progressBar.style.setProperty("transition", "width 250ms ease-out");
          progressBar.style.setProperty("width", `${progressPercent}%`);
        };
        if (win) {
          win.requestAnimationFrame(() =>
            win.requestAnimationFrame(animateProgress),
          );
        } else {
          animateProgress();
        }
        panel.append(progressContainer);
      }

      if (selector) {
        const selectorDiv = doc.createElement("div");
        selectorDiv.className = "nr-webscraper-info-panel__selector";
        selectorDiv.textContent = selector;
        panel.append(selectorDiv);
      }

      if (elementCount !== undefined && elementCount > 1) {
        const countDiv = doc.createElement("div");
        countDiv.className = "nr-webscraper-info-panel__count";
        countDiv.textContent = await this.translationHelper.translate(
          "panelProgressSummary",
          {
            count: elementCount,
          },
        );
        panel.append(countDiv);
      }

      (doc.body ?? doc.documentElement)?.append(panel);
      this.infoPanel = panel;

      // Auto-hide after action completes
      this.infoPanelCleanupTimer = Number(
        timerSetTimeout(() => this.hideInfoPanel(), 4000),
      );
    } catch (error) {
      console.warn("HighlightManager: Failed to show info panel", error);
    }
  }

  /**
   * Hide info panel
   */
  hideInfoPanel(): void {
    if (this.infoPanelCleanupTimer !== null) {
      timerClearTimeout(this.infoPanelCleanupTimer);
    }
    this.infoPanelCleanupTimer = null;

    const panel = this.infoPanel;
    this.infoPanel = null;

    try {
      if (panel?.isConnected) {
        panel.remove();
      }
    } catch {
      // DeadObject - ignore
    }

    // Also cleanup any orphaned info panels in the DOM
    const doc = this.document;
    if (doc) {
      try {
        const orphanedPanels = doc.querySelectorAll(
          ".nr-webscraper-info-panel",
        );
        orphanedPanels.forEach((el) => {
          try {
            el.remove();
          } catch {
            // DeadObject - ignore
          }
        });
      } catch {
        // ignore DOM query errors
      }
    }
  }

  /**
   * Clean up all highlight overlays
   */
  cleanupHighlight(): void {
    ++this.highlightOperationId;
    this.cleanupHighlightWithoutIdIncrement();
  }

  /**
   * Gracefully cleanup highlight - immediately removes old overlays
   * The "graceful" aspect is handled by fade-out animation in cleanupHighlight
   */
  gracefulCleanupHighlight(): void {
    // Always immediately cleanup to prevent orphaned overlays
    // The fade-out animation provides the graceful visual transition
    this.cleanupHighlight();
  }

  /**
   * Internal cleanup without incrementing operation ID
   * Used when the caller has already claimed an operation ID
   */
  private cleanupHighlightWithoutIdIncrement(): void {
    if (this.highlightCleanupTimer !== null) {
      timerClearTimeout(this.highlightCleanupTimer);
    }
    this.highlightCleanupTimer = null;

    // Capture references and immediately clear instance variables
    const overlaysToRemove = [
      ...(this.highlightOverlay ? [this.highlightOverlay] : []),
      ...this.highlightOverlays,
    ];
    const callbacksToRun = [...this.highlightCleanupCallbacks];

    this.highlightOverlay = null;
    this.highlightOverlays = [];
    this.highlightCleanupCallbacks = [];

    // Run cleanup callbacks immediately
    for (const cleanup of callbacksToRun) {
      try {
        cleanup();
      } catch {
        // ignore cleanup errors
      }
    }

    // Also search DOM for any orphaned overlays (exclude fading ones)
    const doc = this.document;
    if (doc) {
      try {
        const orphanedOverlays = doc.querySelectorAll(
          ".nr-webscraper-highlight-overlay:not([data-removing='true'])",
        );
        orphanedOverlays.forEach((el) => {
          if (!overlaysToRemove.includes(el as HTMLDivElement)) {
            overlaysToRemove.push(el as HTMLDivElement);
          }
        });

        // Also cleanup any orphaned info panels
        const orphanedPanels = doc.querySelectorAll(
          ".nr-webscraper-info-panel",
        );
        orphanedPanels.forEach((el) => {
          try {
            if (el !== this.infoPanel) {
              el.remove();
            }
          } catch {
            // DeadObject - ignore
          }
        });
      } catch {
        // ignore DOM query errors
      }
    }

    // Remove overlays with fade-out animation
    this.fadeOutAndRemoveOverlays(overlaysToRemove);
  }

  /**
   * Fade out and remove overlays with animation
   */
  private fadeOutAndRemoveOverlays(overlays: HTMLDivElement[]): void {
    if (overlays.length === 0) return;

    overlays.forEach((overlay) => {
      try {
        if (overlay?.isConnected) {
          // Mark overlay as being removed to prevent interference
          overlay.setAttribute("data-removing", "true");
          overlay.style.setProperty(
            "transition",
            "opacity 300ms ease-out, transform 300ms ease-out",
          );
          overlay.style.setProperty("opacity", "0");
          overlay.style.setProperty("transform", "scale(0.98)");
          timerSetTimeout(() => {
            try {
              if (
                overlay?.isConnected &&
                overlay.getAttribute("data-removing") === "true"
              ) {
                overlay.remove();
              }
            } catch {
              // DeadObject - ignore
            }
          }, 300);
        }
      } catch {
        // DeadObject - ignore
      }
    });
  }

  /**
   * Apply highlight to a single element
   */
  async applyHighlight(
    target: Element | null,
    optionsInput: HighlightOptionsInput = {},
    elementInfo?: string,
    showPanel = true,
  ): Promise<boolean> {
    if (!target) {
      this.cleanupHighlight();
      return false;
    }

    try {
      void target.nodeType;
    } catch {
      this.cleanupHighlight();
      return false;
    }

    const mergedOptions = this.getHighlightOptions(
      optionsInput.action ?? "Highlight",
      optionsInput,
    );
    const options = this.normalizeHighlightOptions(mergedOptions);
    const win = this.contentWindow;
    const doc = this.document;

    if (!win || !doc) {
      return true;
    }

    this.ensureHighlightStyle();

    // Increment operation ID BEFORE cleanup to claim this operation
    // This ensures the ID is stable throughout the async operation
    const currentOperationId = ++this.highlightOperationId;
    this.highlightStartTime = Date.now();

    // Clean up previous overlays (but don't increment ID again)
    this.cleanupHighlightWithoutIdIncrement();

    if (showPanel) {
      await this.showInfoPanel(options.action, undefined, elementInfo, 1);

      if (currentOperationId !== this.highlightOperationId) {
        this.hideInfoPanel();
        return true;
      }
    }

    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-highlight-overlay";

    const colorClass = this.getActionColorClass(options.action);
    if (colorClass) {
      overlay.classList.add(colorClass);
    }

    const padding = options.padding;

    const showLabel =
      options.action && options.action.toLowerCase() !== "inspectpeek";

    if (showLabel) {
      const label = doc.createElement("div");
      label.className = "nr-webscraper-highlight-label";
      label.textContent = await this.translationHelper.getActionLabel(
        options.action,
      );
      overlay.append(label);

      if (currentOperationId !== this.highlightOperationId) {
        this.hideInfoPanel();
        return true;
      }
    }

    (doc.body ?? doc.documentElement)?.append(overlay);
    this.highlightOverlay = overlay;

    const updatePosition = () => {
      const rect = target.getBoundingClientRect();
      const top = Math.max(rect.top - padding, 0);
      const left = Math.max(rect.left - padding, 0);
      const width = Math.max(rect.width + padding * 2, 0);
      const height = Math.max(rect.height + padding * 2, 0);

      overlay.style.setProperty("top", `${top}px`);
      overlay.style.setProperty("left", `${left}px`);
      overlay.style.setProperty("width", `${width}px`);
      overlay.style.setProperty("height", `${height}px`);

      const labelEl = overlay.querySelector(
        ".nr-webscraper-highlight-label",
      ) as HTMLElement | null;
      if (labelEl) {
        if (top < labelEl.offsetHeight + 12) {
          overlay.classList.add("nr-webscraper-highlight-overlay--below");
        } else {
          overlay.classList.remove("nr-webscraper-highlight-overlay--below");
        }
      }
    };

    updatePosition();

    void overlay.getBoundingClientRect();
    overlay.classList.add("nr-webscraper-highlight-overlay--visible");

    const reposition = () => updatePosition();
    win.addEventListener("scroll", reposition, true);
    win.addEventListener("resize", reposition);
    this.highlightCleanupCallbacks.push(() => {
      win.removeEventListener("scroll", reposition, true);
      win.removeEventListener("resize", reposition);
    });

    const mutationObserver = new win.MutationObserver(() => updatePosition());
    mutationObserver.observe(target, {
      attributes: true,
      childList: false,
      subtree: false,
    });
    this.highlightCleanupCallbacks.push(() => mutationObserver.disconnect());

    if (options.scrollBehavior !== "none") {
      try {
        (target as HTMLElement).scrollIntoView({
          behavior: options.scrollBehavior,
          block: "center",
          inline: "center",
        });
      } catch {
        // ignore scroll errors
      }
    }

    if (options.focus && target instanceof HTMLElement) {
      const element = target;
      const prevTabIndex = element.getAttribute("tabindex");
      let tabIndexAdjusted = false;

      if (element.tabIndex < 0) {
        element.setAttribute("tabindex", "-1");
        tabIndexAdjusted = true;
      }

      try {
        // Clone focus options into content context to avoid security wrapper issues
        const win = element.ownerDocument?.defaultView;
        const focusOpts = win
          ? Cu.cloneInto({ preventScroll: true }, win)
          : { preventScroll: true };
        element.focus(focusOpts);
      } catch {
        try {
          element.focus();
        } catch {
          // ignore
        }
      }

      if (tabIndexAdjusted) {
        this.highlightCleanupCallbacks.push(() => {
          if (prevTabIndex === null) {
            element.removeAttribute("tabindex");
          } else {
            element.setAttribute("tabindex", prevTabIndex);
          }
        });
      }
    }

    this.highlightCleanupTimer = Number(
      timerSetTimeout(() => {
        this.cleanupHighlight();
        this.hideInfoPanel();
      }, options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    if (currentOperationId !== this.highlightOperationId) {
      return true;
    }

    if (options.delay > 0) {
      await new Promise<void>((resolve) => {
        timerSetTimeout(() => resolve(), options.delay);
      });
    }

    return true;
  }

  /**
   * Apply highlight to multiple elements
   */
  async applyHighlightMultiple(
    targets: Element[],
    optionsInput: HighlightOptionsInput = {},
    elementInfo?: string,
  ): Promise<boolean> {
    if (!targets || targets.length === 0) {
      this.cleanupHighlight();
      return false;
    }

    const mergedOptions = this.getHighlightOptions(
      optionsInput.action ?? "Highlight",
      optionsInput,
    );
    const options = this.normalizeHighlightOptions(mergedOptions);
    const win = this.contentWindow;
    const doc = this.document;

    if (!win || !doc) {
      return true;
    }

    this.ensureHighlightStyle();

    // Increment operation ID BEFORE cleanup to claim this operation
    // This ensures the ID is stable throughout the async operation
    const currentOperationId = ++this.highlightOperationId;
    this.highlightStartTime = Date.now();

    // Clean up previous overlays (but don't increment ID again)
    this.cleanupHighlightWithoutIdIncrement();

    const colorClass = this.getActionColorClass(options.action);

    await this.showInfoPanel(
      options.action,
      undefined,
      elementInfo,
      targets.length,
      0,
      targets.length,
    );

    if (currentOperationId !== this.highlightOperationId) {
      this.hideInfoPanel();
      return true;
    }

    for (const target of targets) {
      try {
        void target.nodeType;
      } catch {
        continue;
      }

      const overlay = doc.createElement("div");
      overlay.className = "nr-webscraper-highlight-overlay";
      if (colorClass) {
        overlay.classList.add(colorClass);
      }

      const padding = options.padding;

      (doc.body ?? doc.documentElement)?.append(overlay);
      this.highlightOverlays.push(overlay);

      const updatePosition = () => {
        const rect = target.getBoundingClientRect();
        const top = Math.max(rect.top - padding, 0);
        const left = Math.max(rect.left - padding, 0);
        const width = Math.max(rect.width + padding * 2, 0);
        const height = Math.max(rect.height + padding * 2, 0);

        overlay.style.setProperty("top", `${top}px`);
        overlay.style.setProperty("left", `${left}px`);
        overlay.style.setProperty("width", `${width}px`);
        overlay.style.setProperty("height", `${height}px`);
      };

      updatePosition();

      void overlay.getBoundingClientRect();
      overlay.classList.add("nr-webscraper-highlight-overlay--visible");

      const reposition = () => updatePosition();
      win.addEventListener("scroll", reposition, true);
      win.addEventListener("resize", reposition);
      this.highlightCleanupCallbacks.push(() => {
        win.removeEventListener("scroll", reposition, true);
        win.removeEventListener("resize", reposition);
      });

      const mutationObserver = new win.MutationObserver(() => updatePosition());
      mutationObserver.observe(target, {
        attributes: true,
        childList: false,
        subtree: false,
      });
      this.highlightCleanupCallbacks.push(() => mutationObserver.disconnect());
    }

    if (
      options.scrollBehavior !== "none" &&
      targets.length > 0 &&
      this.highlightOverlays.length > 0
    ) {
      try {
        const firstAliveTarget = targets.find((t) => {
          try {
            return t.isConnected;
          } catch {
            return false;
          }
        });
        if (firstAliveTarget) {
          (firstAliveTarget as HTMLElement).scrollIntoView({
            behavior: options.scrollBehavior,
            block: "center",
            inline: "center",
          });
        }
      } catch {
        // ignore scroll errors
      }
    }

    this.highlightCleanupTimer = Number(
      timerSetTimeout(() => {
        // Only cleanup if this operation is still active
        if (currentOperationId === this.highlightOperationId) {
          this.cleanupHighlight();
          this.hideInfoPanel();
        }
      }, options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    if (currentOperationId !== this.highlightOperationId) {
      return true;
    }

    if (options.delay > 0) {
      await new Promise<void>((resolve) => {
        timerSetTimeout(() => resolve(), options.delay);
      });
    }

    return true;
  }

  /**
   * Highlight for inspection (read-only actions)
   */
  async highlightInspection(
    target: Element | Element[],
    elementInfo?: string,
    showPanel = true,
  ): Promise<void> {
    try {
      if (Array.isArray(target)) {
        if (!target.length) {
          return;
        }
        await this.applyHighlightMultiple(
          target,
          { action: "Inspect" },
          elementInfo,
        );
      } else {
        await this.applyHighlight(
          target,
          { action: "Inspect" },
          elementInfo,
          showPanel,
        );
      }
    } catch {
      // ignore highlight errors
    }
  }

  /**
   * Show control overlay that blocks user interaction
   */
  showControlOverlay(): void {
    const doc = this.document;
    const win = this.contentWindow;
    if (!doc || !win) {
      return;
    }

    if (this.controlOverlay?.isConnected) {
      return;
    }

    this.ensureHighlightStyle();

    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-control-overlay";
    overlay.id = "nr-webscraper-control-overlay";
    overlay.tabIndex = 0; // Make focusable

    const label = doc.createElement("div");
    label.className = "nr-webscraper-control-overlay__label";
    label.textContent = "Floorp が操作中...";

    (doc.body ?? doc.documentElement)?.appendChild(overlay);
    (doc.body ?? doc.documentElement)?.appendChild(label);

    // Focus the overlay to capture keyboard events
    overlay.focus();

    const body = doc.body;
    const html = doc.documentElement as HTMLElement | null;
    if (body) body.style.setProperty("overflow", "hidden", "important");
    if (html) html.style.setProperty("overflow", "hidden", "important");

    // Create event handlers that will be added to document level
    const blockEvent = (e: Event) => {
      // Don't block events on the overlay itself
      if (e.target === overlay || e.target === label) return;
      e.preventDefault();
      e.stopPropagation();
      e.stopImmediatePropagation();
    };

    const blockKeydown = (e: KeyboardEvent) => {
      // Block all keyboard input except for specific allowed keys
      const allowedKeys = ["F12"]; // Allow dev tools
      if (!allowedKeys.includes(e.key)) {
        e.preventDefault();
        e.stopPropagation();
        e.stopImmediatePropagation();
      }
    };

    const blockFocus = (e: FocusEvent) => {
      // Redirect focus back to overlay
      if (e.target !== overlay) {
        e.preventDefault();
        e.stopPropagation();
        overlay.focus();
      }
    };

    // Store handlers for cleanup
    this.controlOverlayHandlers = { blockEvent, blockKeydown, blockFocus };

    // Add document-level event listeners with capture phase
    const eventOptions = { capture: true, passive: false };

    // Block mouse events
    doc.addEventListener("mousedown", blockEvent, eventOptions);
    doc.addEventListener("mouseup", blockEvent, eventOptions);
    doc.addEventListener("click", blockEvent, eventOptions);
    doc.addEventListener("dblclick", blockEvent, eventOptions);
    doc.addEventListener("contextmenu", blockEvent, eventOptions);

    // Block touch events
    doc.addEventListener("touchstart", blockEvent, eventOptions);
    doc.addEventListener("touchmove", blockEvent, eventOptions);
    doc.addEventListener("touchend", blockEvent, eventOptions);

    // Block keyboard events
    doc.addEventListener("keydown", blockKeydown, eventOptions);
    doc.addEventListener("keyup", blockEvent, eventOptions);
    doc.addEventListener("keypress", blockEvent, eventOptions);

    // Block scroll events
    doc.addEventListener("wheel", blockEvent, eventOptions);
    doc.addEventListener("scroll", blockEvent, eventOptions);

    // Block focus events
    doc.addEventListener("focus", blockFocus, eventOptions);
    doc.addEventListener("focusin", blockFocus, eventOptions);

    // Block input events
    doc.addEventListener("input", blockEvent, eventOptions);
    doc.addEventListener("change", blockEvent, eventOptions);

    // Block drag events
    doc.addEventListener("dragstart", blockEvent, eventOptions);
    doc.addEventListener("drag", blockEvent, eventOptions);
    doc.addEventListener("drop", blockEvent, eventOptions);

    this.controlOverlay = overlay;
    this.controlOverlayLabel = label;
  }

  /**
   * Hide control overlay
   */
  hideControlOverlay(): void {
    const doc = this.document;
    if (!doc) return;

    // Remove document-level event listeners
    if (this.controlOverlayHandlers) {
      const { blockEvent, blockKeydown, blockFocus } =
        this.controlOverlayHandlers;
      const eventOptions = { capture: true };

      // Remove mouse events
      doc.removeEventListener("mousedown", blockEvent, eventOptions);
      doc.removeEventListener("mouseup", blockEvent, eventOptions);
      doc.removeEventListener("click", blockEvent, eventOptions);
      doc.removeEventListener("dblclick", blockEvent, eventOptions);
      doc.removeEventListener("contextmenu", blockEvent, eventOptions);

      // Remove touch events
      doc.removeEventListener("touchstart", blockEvent, eventOptions);
      doc.removeEventListener("touchmove", blockEvent, eventOptions);
      doc.removeEventListener("touchend", blockEvent, eventOptions);

      // Remove keyboard events
      doc.removeEventListener("keydown", blockKeydown, eventOptions);
      doc.removeEventListener("keyup", blockEvent, eventOptions);
      doc.removeEventListener("keypress", blockEvent, eventOptions);

      // Remove scroll events
      doc.removeEventListener("wheel", blockEvent, eventOptions);
      doc.removeEventListener("scroll", blockEvent, eventOptions);

      // Remove focus events
      doc.removeEventListener("focus", blockFocus, eventOptions);
      doc.removeEventListener("focusin", blockFocus, eventOptions);

      // Remove input events
      doc.removeEventListener("input", blockEvent, eventOptions);
      doc.removeEventListener("change", blockEvent, eventOptions);

      // Remove drag events
      doc.removeEventListener("dragstart", blockEvent, eventOptions);
      doc.removeEventListener("drag", blockEvent, eventOptions);
      doc.removeEventListener("drop", blockEvent, eventOptions);

      this.controlOverlayHandlers = null;
    }

    if (this.controlOverlay?.isConnected) {
      const body = doc.body;
      const html = doc.documentElement as HTMLElement | null;
      if (body) body.style.removeProperty("overflow");
      if (html) html.style.removeProperty("overflow");

      this.controlOverlay.style.setProperty(
        "transition",
        "opacity 300ms ease-out",
      );
      this.controlOverlay.style.setProperty("opacity", "0");
      const overlayToRemove = this.controlOverlay;
      timerSetTimeout(() => {
        try {
          overlayToRemove?.remove();
        } catch {
          // ignore
        }
      }, 300);
      this.controlOverlay = null;
    }

    if (this.controlOverlayLabel?.isConnected) {
      this.controlOverlayLabel.style.setProperty(
        "transition",
        "opacity 200ms ease-out",
      );
      this.controlOverlayLabel.style.setProperty("opacity", "0");
      const labelToRemove = this.controlOverlayLabel;
      timerSetTimeout(() => {
        try {
          labelToRemove?.remove();
        } catch {
          // ignore
        }
      }, 200);
      this.controlOverlayLabel = null;
    }
  }

  /**
   * Run async inspection highlighting without blocking
   */
  runAsyncInspection(
    target: Element | Element[],
    translationKey: string,
    translationVars: Record<string, string | number> = {},
  ): void {
    const startOperationId = this.highlightOperationId;

    void (async () => {
      try {
        if (Array.isArray(target)) {
          const aliveTargets = target.filter((el) => {
            try {
              return el.isConnected;
            } catch {
              return false;
            }
          });
          if (aliveTargets.length === 0) return;
        } else {
          try {
            if (!target.isConnected) return;
          } catch {
            return;
          }
        }

        const elementInfo = await this.translationHelper.translate(
          translationKey,
          translationVars,
        );

        if (startOperationId !== this.highlightOperationId) {
          return;
        }

        await this.highlightInspection(target, elementInfo);
      } catch {
        // Silently ignore inspection errors
      }
    })();
  }

  /**
   * Lightweight highlight for value reads
   */
  runAsyncValuePeek(target: Element): void {
    const startOperationId = this.highlightOperationId;

    void (async () => {
      try {
        if (!target.isConnected) {
          return;
        }

        if (startOperationId !== this.highlightOperationId) {
          return;
        }

        await this.applyHighlight(
          target,
          { action: "InspectPeek" },
          undefined,
          false,
        );
      } catch {
        // Silently ignore highlight errors
      }
    })();
  }

  /**
   * Cleanup all resources
   */
  destroy(): void {
    this.cleanupHighlight();
    this.hideInfoPanel();
    this.hideControlOverlay();

    try {
      if (this.highlightStyleElement?.isConnected) {
        this.highlightStyleElement.remove();
      }
    } catch {
      // DeadObject - ignore
    }
    this.highlightStyleElement = null;
  }
}
