/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRWebScraperChild - Content process actor for web scraping operations
 *
 * This actor runs in the content process and provides functionality to:
 * - Extract HTML content from web pages
 * - Interact with DOM elements (input fields, textareas)
 * - Handle messages from the parent process WebScraper service
 * - Provide safe access to content window and document objects
 *
 * The actor is automatically created for each browser tab/content window
 * and communicates with the parent process through message passing.
 */
type HighlightScrollBehavior = ScrollBehavior | "none";

interface HighlightRequest {
  action?: string;
  duration?: number;
  focus?: boolean;
  scrollBehavior?: HighlightScrollBehavior;
  padding?: number;
  delay?: number;
}

interface NRWebScraperMessageData {
  selector?: string;
  value?: string;
  textContent?: string;
  timeout?: number;
  script?: string;
  rect?: { x?: number; y?: number; width?: number; height?: number };
  formData?: { [selector: string]: string };
  highlight?: HighlightRequest;
  elementInfo?: string;
}

interface NormalizedHighlightOptions {
  action?: string;
  duration: number;
  focus: boolean;
  scrollBehavior: HighlightScrollBehavior;
  padding: number;
  delay: number;
}

type HighlightFocusOptions = {
  preventScroll?: boolean;
};

export class NRWebScraperChild extends JSWindowActorChild {
  private highlightOverlay: HTMLDivElement | null = null;
  private highlightOverlays: HTMLDivElement[] = [];
  private highlightCleanupTimer: number | null = null;
  private highlightCleanupCallbacks: Array<() => void> = [];
  private highlightStyleElement: HTMLStyleElement | null = null;
  private persistentStyleElement: HTMLStyleElement | null = null;
  private persistentEffects = new Set<Element>();
  private persistentWindowListenersRegistered = false;
  private persistentCleanupHandler: ((event: Event) => void) | null = null;
  private infoPanel: HTMLDivElement | null = null;
  private infoPanelCleanupTimer: number | null = null;

  private normalizeHighlightOptions(
    highlight?: HighlightRequest,
  ): NormalizedHighlightOptions | null {
    if (!highlight) {
      return null;
    }

    return {
      action: highlight.action,
      duration: Math.max(highlight.duration ?? 1400, 200),
      focus: highlight.focus ?? true,
      scrollBehavior: highlight.scrollBehavior ?? "smooth",
      padding: Math.max(highlight.padding ?? 12, 0),
      delay: Math.max(highlight.delay ?? 350, 0),
    };
  }

  private ensureHighlightStyle(): void {
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
    style.textContent = `@keyframes nr-webscraper-highlight-pulse {
  0% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-45);
    transform: scale(1);
  }
  50% {
    box-shadow: 0 0 0 8px var(--nr-highlight-color-alpha-25);
    transform: scale(1.015);
  }
  100% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-0);
    transform: scale(1);
  }
}

@keyframes nr-webscraper-info-slide-in {
  from {
    opacity: 0;
    transform: translateY(-20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.nr-webscraper-highlight-overlay {
  --nr-highlight-color-alpha-0: rgba(59, 130, 246, 0);
  --nr-highlight-color-alpha-25: rgba(59, 130, 246, 0.25);
  --nr-highlight-color-alpha-45: rgba(59, 130, 246, 0.45);
  --nr-highlight-color: rgba(59, 130, 246, 0.95);
  --nr-highlight-bg: rgba(59, 130, 246, 0.08);
  --nr-label-bg: rgba(37, 99, 235, 0.94);
  position: fixed;
  border-radius: 10px;
  border: 2px solid var(--nr-highlight-color);
  box-shadow: 0 0 0 4px var(--nr-highlight-color-alpha-25);
  pointer-events: none;
  z-index: 2147483645;
  opacity: 0;
  transform: scale(0.98);
  transition: opacity 120ms ease-out, transform 120ms ease-out;
  background: var(--nr-highlight-bg);
}

.nr-webscraper-highlight-overlay--read {
  --nr-highlight-color-alpha-0: rgba(34, 197, 94, 0);
  --nr-highlight-color-alpha-25: rgba(34, 197, 94, 0.25);
  --nr-highlight-color-alpha-45: rgba(34, 197, 94, 0.45);
  --nr-highlight-color: rgba(34, 197, 94, 0.95);
  --nr-highlight-bg: rgba(34, 197, 94, 0.08);
  --nr-label-bg: rgba(22, 163, 74, 0.94);
}

.nr-webscraper-highlight-overlay--write {
  --nr-highlight-color-alpha-0: rgba(168, 85, 247, 0);
  --nr-highlight-color-alpha-25: rgba(168, 85, 247, 0.25);
  --nr-highlight-color-alpha-45: rgba(168, 85, 247, 0.45);
  --nr-highlight-color: rgba(168, 85, 247, 0.95);
  --nr-highlight-bg: rgba(168, 85, 247, 0.08);
  --nr-label-bg: rgba(147, 51, 234, 0.94);
}

.nr-webscraper-highlight-overlay--click {
  --nr-highlight-color-alpha-0: rgba(249, 115, 22, 0);
  --nr-highlight-color-alpha-25: rgba(249, 115, 22, 0.25);
  --nr-highlight-color-alpha-45: rgba(249, 115, 22, 0.45);
  --nr-highlight-color: rgba(249, 115, 22, 0.95);
  --nr-highlight-bg: rgba(249, 115, 22, 0.08);
  --nr-label-bg: rgba(234, 88, 12, 0.94);
}

.nr-webscraper-highlight-overlay--submit {
  --nr-highlight-color-alpha-0: rgba(239, 68, 68, 0);
  --nr-highlight-color-alpha-25: rgba(239, 68, 68, 0.25);
  --nr-highlight-color-alpha-45: rgba(239, 68, 68, 0.45);
  --nr-highlight-color: rgba(239, 68, 68, 0.95);
  --nr-highlight-bg: rgba(239, 68, 68, 0.08);
  --nr-label-bg: rgba(220, 38, 38, 0.94);
}

.nr-webscraper-highlight-overlay.nr-webscraper-highlight-overlay--visible {
  opacity: 1;
  transform: scale(1);
  animation: nr-webscraper-highlight-pulse 950ms ease-in-out 1;
}

.nr-webscraper-highlight-label {
  position: absolute;
  top: -28px;
  left: 0;
  transform: translateY(-4px);
  background: var(--nr-label-bg);
  color: #fff;
  padding: 4px 10px;
  border-radius: 9999px;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 12px;
  font-weight: 600;
  white-space: nowrap;
  box-shadow: 0 10px 25px rgba(30, 64, 175, 0.25);
}

.nr-webscraper-highlight-overlay.nr-webscraper-highlight-overlay--below .nr-webscraper-highlight-label {
  top: auto;
  bottom: -28px;
  transform: translateY(4px);
}

.nr-webscraper-info-panel {
  position: fixed;
  top: 16px;
  right: 16px;
  max-width: 380px;
  background: rgba(17, 24, 39, 0.96);
  backdrop-filter: blur(12px);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 12px;
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.3), 0 0 0 1px rgba(255, 255, 255, 0.05);
  pointer-events: none;
  z-index: 2147483647;
  animation: nr-webscraper-info-slide-in 200ms ease-out;
  color: #fff;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 13px;
  padding: 14px 16px;
}

.nr-webscraper-info-panel__title {
  font-weight: 700;
  font-size: 14px;
  margin-bottom: 8px;
  display: flex;
  align-items: center;
  gap: 8px;
  color: #fff;
}

.nr-webscraper-info-panel__badge {
  display: inline-flex;
  align-items: center;
  padding: 2px 8px;
  border-radius: 9999px;
  font-size: 11px;
  font-weight: 600;
  background: var(--nr-label-bg);
  color: #fff;
}

.nr-webscraper-info-panel__content {
  color: rgba(255, 255, 255, 0.85);
  line-height: 1.5;
}

.nr-webscraper-info-panel__selector {
  margin-top: 8px;
  padding: 8px 10px;
  background: rgba(0, 0, 0, 0.3);
  border-radius: 6px;
  font-family: "SF Mono", Monaco, Consolas, monospace;
  font-size: 12px;
  color: rgba(168, 85, 247, 0.95);
  word-break: break-all;
  border: 1px solid rgba(255, 255, 255, 0.05);
}

.nr-webscraper-info-panel__count {
  margin-top: 6px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
}`;

    (doc.head ?? doc.documentElement)?.append(style);
    this.highlightStyleElement = style;
  }

  private cleanupHighlight(): void {
    const win = this.contentWindow;

    if (this.highlightCleanupTimer !== null && win) {
      win.clearTimeout(this.highlightCleanupTimer);
    }
    this.highlightCleanupTimer = null;

    for (const cleanup of this.highlightCleanupCallbacks) {
      try {
        cleanup();
      } catch (_) {
        // ignore cleanup errors
      }
    }
    this.highlightCleanupCallbacks = [];

    if (this.highlightOverlay?.isConnected) {
      this.highlightOverlay.remove();
    }
    this.highlightOverlay = null;

    // Cleanup multiple overlays
    for (const overlay of this.highlightOverlays) {
      if (overlay?.isConnected) {
        overlay.remove();
      }
    }
    this.highlightOverlays = [];
  }

  private getActionColorClass(action?: string): string {
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

  private showInfoPanel(
    action: string,
    selector?: string,
    elementInfo?: string,
    elementCount?: number,
  ): void {
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

      const badge = doc.createElement("span");
      badge.className = "nr-webscraper-info-panel__badge";
      badge.textContent = action;

      title.append(badge);
      title.append(doc.createTextNode("Operation in Progress"));

      const content = doc.createElement("div");
      content.className = "nr-webscraper-info-panel__content";

      if (elementInfo) {
        content.textContent = elementInfo;
      } else {
        content.textContent = `Performing ${action.toLowerCase()} operation...`;
      }

      panel.append(title);
      panel.append(content);

      if (selector) {
        const selectorDiv = doc.createElement("div");
        selectorDiv.className = "nr-webscraper-info-panel__selector";
        selectorDiv.textContent = selector;
        panel.append(selectorDiv);
      }

      if (elementCount !== undefined && elementCount > 1) {
        const countDiv = doc.createElement("div");
        countDiv.className = "nr-webscraper-info-panel__count";
        countDiv.textContent = `${elementCount} element${elementCount !== 1 ? "s" : ""} affected`;
        panel.append(countDiv);
      }

      (doc.body ?? doc.documentElement)?.append(panel);
      this.infoPanel = panel;

      // Auto-hide after action completes
      this.infoPanelCleanupTimer = Number(
        win.setTimeout(() => this.hideInfoPanel(), 3000),
      );
    } catch (error) {
      console.warn("NRWebScraperChild: Failed to show info panel", error);
    }
  }

  private hideInfoPanel(): void {
    const win = this.contentWindow;
    if (this.infoPanelCleanupTimer !== null && win) {
      win.clearTimeout(this.infoPanelCleanupTimer);
    }
    this.infoPanelCleanupTimer = null;

    if (this.infoPanel?.isConnected) {
      this.infoPanel.remove();
    }
    this.infoPanel = null;
  }

  private ensurePersistentStyle(): void {
    const doc = this.document;
    if (!doc) {
      return;
    }

    if (this.persistentStyleElement?.isConnected) {
      return;
    }

    const existing = doc.getElementById("nr-webscraper-persistent-style");
    if (existing instanceof HTMLStyleElement) {
      this.persistentStyleElement = existing;
      return;
    }

    const style = doc.createElement("style");
    style.id = "nr-webscraper-persistent-style";
    style.textContent = `.nr-webscraper-effect {
  outline: 3px solid rgba(37, 99, 235, 0.85);
  outline-offset: 2px;
  box-shadow: 0 0 12px rgba(37, 99, 235, 0.35);
  transition: outline-color 180ms ease-out, box-shadow 180ms ease-out;
  position: relative;
}

.nr-webscraper-effect[data-nr-webscraper-effect]:not([data-nr-webscraper-effect=""])::after {
  content: attr(data-nr-webscraper-effect);
  position: absolute;
  top: -28px;
  left: 0;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: rgba(37, 99, 235, 0.92);
  color: #fff;
  padding: 3px 10px;
  border-radius: 9999px;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 11px;
  font-weight: 600;
  white-space: nowrap;
  pointer-events: none;
  transform: translateY(-4px);
  box-shadow: 0 8px 16px rgba(30, 64, 175, 0.25);
  z-index: 2147483646;
}

.nr-webscraper-effect[data-nr-webscraper-effect-position="below"]::after {
  top: auto;
  bottom: -28px;
  transform: translateY(4px);
}`;

    (doc.head ?? doc.documentElement)?.append(style);
    this.persistentStyleElement = style;
  }

  private ensureWindowListeners(): void {
    if (this.persistentWindowListenersRegistered) {
      return;
    }
    const win = this.contentWindow;
    if (!win) {
      return;
    }

    const cleanup = (_event: Event) => {
      this.clearPersistentEffects();
    };

    win.addEventListener("pagehide", cleanup, true);
    win.addEventListener("beforeunload", cleanup, true);
    win.addEventListener("unload", cleanup, true);
    this.persistentCleanupHandler = cleanup;
    this.persistentWindowListenersRegistered = true;
  }

  private applyPersistentEffect(
    element: Element | null,
    action?: string,
  ): void {
    try {
      if (!element) {
        return;
      }

      this.clearPersistentEffects({ keepStyle: true });

      this.ensurePersistentStyle();
      this.ensureWindowListeners();

      element.classList.add("nr-webscraper-effect");
      if (action) {
        element.setAttribute("data-nr-webscraper-effect", action);
      } else {
        element.removeAttribute("data-nr-webscraper-effect");
      }

      const rect = element.getBoundingClientRect?.();
      if (rect) {
        if (rect.top < 32) {
          element.setAttribute("data-nr-webscraper-effect-position", "below");
        } else {
          element.removeAttribute("data-nr-webscraper-effect-position");
        }
      }

      this.persistentEffects.add(element);
    } catch (error) {
      console.warn(
        "NRWebScraperChild: Failed to apply persistent effect",
        error,
      );
    }
  }

  private clearPersistentEffects(options?: { keepStyle?: boolean }): void {
    const keepStyle = options?.keepStyle ?? false;

    this.cleanupHighlight();

    for (const element of this.persistentEffects) {
      try {
        element.classList.remove("nr-webscraper-effect");
        element.removeAttribute("data-nr-webscraper-effect");
        element.removeAttribute("data-nr-webscraper-effect-position");
      } catch (error) {
        console.warn("NRWebScraperChild: Failed to clear effect", error);
      }
    }
    this.persistentEffects.clear();

    if (!keepStyle) {
      const win = this.contentWindow;
      if (win && this.persistentCleanupHandler) {
        try {
          win.removeEventListener(
            "pagehide",
            this.persistentCleanupHandler,
            true,
          );
          win.removeEventListener(
            "beforeunload",
            this.persistentCleanupHandler,
            true,
          );
          win.removeEventListener(
            "unload",
            this.persistentCleanupHandler,
            true,
          );
        } catch (_) {
          // ignore removal errors
        }
      }
      this.persistentCleanupHandler = null;
      this.persistentWindowListenersRegistered = false;

      if (this.persistentStyleElement?.isConnected) {
        try {
          this.persistentStyleElement.remove();
        } catch (_) {
          // ignore removal errors
        }
      }
      this.persistentStyleElement = null;
    }
  }

  private async applyHighlightMultiple(
    targets: Element[],
    highlight?: HighlightRequest,
    elementInfo?: string,
  ): Promise<boolean> {
    if (!targets || targets.length === 0) {
      this.cleanupHighlight();
      return false;
    }

    const options = this.normalizeHighlightOptions(highlight);
    const win = this.contentWindow;
    const doc = this.document;

    if (!options || !win || !doc) {
      return true;
    }

    this.ensureHighlightStyle();
    this.cleanupHighlight();

    const colorClass = this.getActionColorClass(options.action);

    // Show info panel
    this.showInfoPanel(
      options.action ?? "Highlight",
      undefined,
      elementInfo,
      targets.length,
    );

    for (const target of targets) {
      const overlay = doc.createElement("div");
      overlay.className = "nr-webscraper-highlight-overlay";
      if (colorClass) {
        overlay.classList.add(colorClass);
      }

      const padding = options.padding;

      (doc.body ?? doc.documentElement)?.append(overlay);

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

      // Force a layout
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

      this.highlightOverlays.push(overlay);
    }

    // Scroll to first element
    if (options.scrollBehavior !== "none" && targets.length > 0) {
      try {
        (targets[0] as HTMLElement).scrollIntoView({
          behavior: options.scrollBehavior,
          block: "center",
          inline: "center",
        });
      } catch (_) {
        // ignore scroll errors
      }
    }

    this.highlightCleanupTimer = Number(
      win.setTimeout(() => this.cleanupHighlight(), options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    if (options.delay > 0) {
      await new Promise<void>((resolve) => {
        win.setTimeout(() => resolve(), options.delay);
      });
    }

    return true;
  }

  private async applyHighlight(
    target: Element | null,
    highlight?: HighlightRequest,
    elementInfo?: string,
  ): Promise<boolean> {
    if (!target) {
      this.cleanupHighlight();
      return false;
    }

    const options = this.normalizeHighlightOptions(highlight);
    const win = this.contentWindow;
    const doc = this.document;

    if (!options || !win || !doc) {
      return true;
    }

    this.ensureHighlightStyle();
    this.cleanupHighlight();

    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-highlight-overlay";

    const colorClass = this.getActionColorClass(options.action);
    if (colorClass) {
      overlay.classList.add(colorClass);
    }

    // Show info panel
    this.showInfoPanel(
      options.action ?? "Highlight",
      undefined,
      elementInfo,
      1,
    );

    const padding = options.padding;

    if (options.action) {
      const label = doc.createElement("div");
      label.className = "nr-webscraper-highlight-label";
      label.textContent = options.action;
      overlay.append(label);
    }

    (doc.body ?? doc.documentElement)?.append(overlay);

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

    // Force a layout so the transition runs consistently
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

    this.highlightOverlay = overlay;

    if (options.scrollBehavior !== "none") {
      try {
        (target as HTMLElement).scrollIntoView({
          behavior: options.scrollBehavior,
          block: "center",
          inline: "center",
        });
      } catch (_) {
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
        element.focus({ preventScroll: true } as HighlightFocusOptions);
      } catch (_) {
        try {
          element.focus();
        } catch (e) {
          void e;
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
      win.setTimeout(() => this.cleanupHighlight(), options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    if (options.delay > 0) {
      await new Promise<void>((resolve) => {
        win.setTimeout(() => resolve(), options.delay);
      });
    }

    return true;
  }
  /**
   * Called when the actor is created for a content window
   *
   * This method is invoked when the actor is instantiated for a new
   * content window. It logs the creation for debugging purposes.
   */
  actorCreated() {
    console.log(
      "NRWebScraperChild created for:",
      this.contentWindow?.location?.href,
    );
  }

  /**
   * Handles incoming messages from the parent process
   *
   * This method processes different types of scraping operations:
   * - "WebScraper:GetHTML": Extracts the full HTML content of the page
   * - "WebScraper:GetElement": Gets a specific element by selector
   * - "WebScraper:GetElementText": Gets text content of an element
   * - "WebScraper:InputElement": Sets values for input elements or textareas
   * - "WebScraper:ClickElement": Clicks on an element
   * - "WebScraper:WaitForElement": Waits for an element to appear
   * - "WebScraper:TakeScreenshot": Takes a screenshot of the viewport
   * - "WebScraper:TakeElementScreenshot": Takes a screenshot of a specific element
   * - "WebScraper:TakeFullPageScreenshot": Takes a screenshot of the full page
   * - "WebScraper:TakeRegionScreenshot": Takes a screenshot of a specific region
   * - "WebScraper:FillForm": Fills multiple form fields at once
   *
   * @param message - The message object containing the operation name and optional data
   * @returns string | null - Returns HTML content for GetHTML operations, null otherwise
   */
  receiveMessage(message: { name: string; data?: NRWebScraperMessageData }) {
    switch (message.name) {
      case "WebScraper:WaitForReady": {
        const to = message.data?.timeout || 15000;
        return this.waitForReady(to);
      }
      case "WebScraper:GetHTML":
        return this.getHTML();
      case "WebScraper:GetElements":
        if (message.data?.selector) {
          return this.getElements(message.data.selector);
        }
        break;
      case "WebScraper:GetElementByText":
        if (message.data?.textContent) {
          return this.getElementByText(message.data.textContent);
        }
        break;
      case "WebScraper:GetElementTextContent":
        if (message.data?.selector) {
          return this.getElementTextContent(message.data.selector);
        }
        break;
      case "WebScraper:GetElement":
        if (message.data?.selector) {
          return this.getElement(message.data.selector);
        }
        break;
      case "WebScraper:GetElementText":
        if (message.data?.selector) {
          return this.getElementText(message.data.selector);
        }
        break;
      case "WebScraper:GetValue":
        if (message.data?.selector) {
          return this.getValue(message.data.selector);
        }
        break;
      case "WebScraper:InputElement":
        if (message.data?.selector && typeof message.data.value === "string") {
          return this.inputElement(
            message.data.selector,
            message.data.value,
            message.data.highlight,
          );
        }
        break;
      case "WebScraper:ClickElement":
        if (message.data?.selector) {
          return this.clickElement(
            message.data.selector,
            message.data.highlight,
          );
        }
        break;
      case "WebScraper:WaitForElement":
        if (message.data?.selector) {
          return this.waitForElement(
            message.data.selector,
            message.data.timeout || 5000,
          );
        }
        break;
      case "WebScraper:TakeScreenshot":
        return this.takeScreenshot();
      case "WebScraper:TakeElementScreenshot":
        if (message.data?.selector) {
          return this.takeElementScreenshot(message.data.selector);
        }
        break;
      case "WebScraper:TakeFullPageScreenshot":
        return this.takeFullPageScreenshot();
      case "WebScraper:TakeRegionScreenshot":
        if (message.data) {
          return this.takeRegionScreenshot(message.data.rect || {});
        }
        break;
      case "WebScraper:FillForm":
        if (message.data?.formData) {
          return this.fillForm(message.data.formData, message.data.highlight);
        }
        break;
      case "WebScraper:Submit":
        if (message.data?.selector) {
          return this.submit(message.data.selector, message.data.highlight);
        }
        break;
    }
    return null;
  }

  /**
   * Extracts the HTML content from the current page
   *
   * This method safely accesses the content window's document and returns
   * the outerHTML of the document element. It includes error handling to
   * prevent crashes when the content window or document is not available.
   *
   * @returns string | null - The HTML content as a string, or null if unavailable
   */
  getHTML(): string | null {
    try {
      if (this.contentWindow && this.contentWindow.document) {
        const html = this.contentWindow.document?.documentElement?.outerHTML;

        console.log("NRWebScraperChild: getHTML", html);

        if (!html) {
          return null;
        }
        return html.toString();
      }

      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting HTML:", e);
      return null;
    }
  }

  /**
   * Gets a specific element by CSS selector
   *
   * This method finds an element using the provided CSS selector and returns
   * its outerHTML. It includes error handling to prevent crashes.
   *
   * @param selector - CSS selector to find the target element
   * @returns string | null - The element's HTML content, or null if not found
   */
  getElement(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector);
      if (element) {
        this.applyPersistentEffect(element, "Inspect");
        return String((element as Element).outerHTML);
      }
      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting element:", e);
      return null;
    }
  }

  /**
   * Returns an array of outerHTML strings for all elements matching selector
   */
  getElements(selector: string): string[] {
    try {
      const nodeList = this.document?.querySelectorAll(selector) as
        | NodeListOf<Element>
        | undefined;
      if (!nodeList) return [];
      const results: string[] = [];
      nodeList.forEach((el) => {
        const o = (el as Element).outerHTML;
        if (o != null) results.push(String(o));
      });
      const first = nodeList.item(0);
      if (first) {
        this.applyPersistentEffect(first, "Inspect");
      }
      return results;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting elements:", e);
      return [];
    }
  }

  /**
   * Returns the outerHTML of the first element whose textContent includes the provided text
   */
  getElementByText(textContent: string): string | null {
    try {
      const nodeList = this.document?.querySelectorAll("*") as
        | NodeListOf<Element>
        | undefined;
      if (!nodeList) return null;
      const elArray = Array.from(nodeList as NodeListOf<Element>) as Element[];
      for (const el of elArray) {
        const txt = el.textContent;
        if (txt && txt.includes(textContent)) {
          this.applyPersistentEffect(el, "Inspect");
          return String(el.outerHTML);
        }
      }
      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting element by text:", e);
      return null;
    }
  }

  /**
   * Returns trimmed textContent of the first matching selector
   */
  getElementTextContent(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector);
      if (element) {
        this.applyPersistentEffect(element, "Read");
        return element.textContent?.trim() || null;
      }
      return null;
    } catch (e) {
      console.error(
        "NRWebScraperChild: Error getting element text content:",
        e,
      );
      return null;
    }
  }

  /**
   * Gets the text content of an element by CSS selector
   *
   * This method finds an element using the provided CSS selector and returns
   * its text content (with HTML tags removed). It includes error handling.
   *
   * @param selector - CSS selector to find the target element
   * @returns string | null - The element's text content, or null if not found
   */
  getElementText(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector);
      if (element) {
        this.applyPersistentEffect(element, "Read");
        return element.textContent?.trim() || null;
      }
      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting element text:", e);
      return null;
    }
  }

  /**
   * Sets the value of an input element or textarea on the page
   *
   * This method finds an element using the provided CSS selector and sets
   * its value. It supports both HTMLInputElement and HTMLTextAreaElement types.
   * The operation is performed safely with error handling to prevent crashes.
   *
   * @param selector - CSS selector to find the target element
   * @param value - The value to set in the element
   */
  async inputElement(
    selector: string,
    value: string,
    highlight?: HighlightRequest,
  ): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (!element) {
        return false;
      }

      const elementInfo = `Setting input value to: "${value.substring(0, 50)}${value.length > 50 ? "..." : ""}"`;
      await this.applyHighlight(element, highlight, elementInfo);

      element.value = value;
      // Trigger input event to ensure the change is detected
      element.dispatchEvent(new Event("input", { bubbles: true }));
      // Some sites update bound state on change/blur
      element.dispatchEvent(new Event("change", { bubbles: true }));
      element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));
      this.applyPersistentEffect(element, highlight?.action ?? "Input");
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error setting input value:", e);
      return false;
    }
  }

  /**
   * Clicks on an element by CSS selector
   *
   * This method finds an element using the provided CSS selector and
   * simulates a click event. It includes error handling and ensures
   * the element is clickable before attempting to click.
   *
   * @param selector - CSS selector to find the target element
   * @returns boolean - True if click was successful, false otherwise
   */
  async clickElement(
    selector: string,
    highlight?: HighlightRequest,
  ): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementTagName = element.tagName?.toLowerCase() || "element";
      const elementText = element.textContent?.trim().substring(0, 30) || "";
      const elementInfo = elementText
        ? `Clicking ${elementTagName}: "${elementText}${elementText.length >= 30 ? "..." : ""}"`
        : `Clicking ${elementTagName} element`;

      await this.applyHighlight(element, highlight, elementInfo);

      if (!highlight) {
        try {
          element.scrollIntoView({ block: "center", inline: "center" });
        } catch {
          /* ignore */
        }

        try {
          (element as unknown as { focus?: () => void }).focus?.();
        } catch {
          /* ignore */
        }
      }

      const win = this.contentWindow ?? null;
      const Ev = (win?.Event ?? globalThis.Event) as typeof Event;
      const MouseEv = (win?.MouseEvent ?? globalThis.MouseEvent ?? null) as
        | typeof MouseEvent
        | null;

      const tagName = (element.tagName || "").toUpperCase();
      const isInput = tagName === "INPUT";
      const isButton = tagName === "BUTTON";
      const isLink = tagName === "A";
      const inputType = isInput
        ? ((element as HTMLInputElement).type || "").toLowerCase()
        : "";

      const triggerInputEvents = () => {
        try {
          element.dispatchEvent(new Ev("input", { bubbles: true }));
          element.dispatchEvent(new Ev("change", { bubbles: true }));
        } catch (err) {
          console.warn("NRWebScraperChild: input/change dispatch failed", err);
        }
      };

      let stateChanged = false;

      if (isInput) {
        const inputEl = element as HTMLInputElement;
        if (inputType === "checkbox") {
          inputEl.checked = !inputEl.checked;
          triggerInputEvents();
          stateChanged = true;
        } else if (inputType === "radio") {
          if (!inputEl.checked) {
            inputEl.checked = true;
            triggerInputEvents();
          }
          stateChanged = true;
        }
      }

      let clickDispatched = false;
      try {
        element.click();
        clickDispatched = true;
      } catch (e) {
        void e;
      }

      if (!clickDispatched && MouseEv) {
        try {
          element.dispatchEvent(
            new MouseEv("click", {
              bubbles: true,
              cancelable: true,
              composed: true,
              view: win ?? undefined,
            }),
          );
          clickDispatched = true;
        } catch (err) {
          console.warn("NRWebScraperChild: synthetic click failed", err);
        }
      }

      if ((isButton || isLink) && !clickDispatched) {
        try {
          element.dispatchEvent(
            new Ev("submit", { bubbles: true, cancelable: true }),
          );
        } catch (err) {
          console.warn("NRWebScraperChild: submit dispatch failed", err);
        }
      }

      const success = stateChanged || clickDispatched;
      if (success) {
        this.applyPersistentEffect(element, highlight?.action ?? "Click");
      }

      return success;
    } catch (e) {
      console.error("NRWebScraperChild: Error clicking element:", e);
      return false;
    }
  }

  /**
   * Waits for an element to appear in the DOM
   *
   * This method polls the DOM for the presence of an element matching
   * the provided CSS selector. It returns when the element is found
   * or when the timeout is reached.
   *
   * @param selector - CSS selector to find the target element
   * @param timeout - Maximum time to wait in milliseconds (default: 5000)
   * @returns boolean - True if element was found, false if timeout reached
   */
  async waitForElement(selector: string, timeout = 5000): Promise<boolean> {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      try {
        const element = this.document?.querySelector(selector);
        if (element) {
          return true;
        }
        // Wait 100ms before next check
        await new Promise((resolve) => setTimeout(resolve, 100));
      } catch (e) {
        console.error("NRWebScraperChild: Error waiting for element:", e);
        return false;
      }
    }

    return false;
  }

  /**
   * Waits until the document is minimally ready for scraping.
   * Conditions:
   * - document and documentElement exist, and
   * - body exists, or readyState is at least 'interactive',
   * within the specified timeout.
   */
  async waitForReady(timeout = 15000): Promise<boolean> {
    const start = Date.now();
    while (Date.now() - start < timeout) {
      try {
        const win = this.contentWindow;
        const doc = win?.document;
        if (
          doc &&
          doc.documentElement &&
          (doc.body ||
            doc.readyState === "interactive" ||
            doc.readyState === "complete")
        ) {
          return true;
        }
        await new Promise((r) => setTimeout(r, 100));
      } catch (e) {
        void e;
        await new Promise((r) => setTimeout(r, 100));
      }
    }
    return false;
  }

  /**
   * Takes a screenshot of the current viewport
   *
   * This method captures the visible area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   */
  async takeScreenshot(): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const canvas = (await this.contentWindow.document?.createElement(
        "canvas",
      )) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
      const { innerWidth, innerHeight } = this.contentWindow;

      canvas.width = innerWidth;
      canvas.height = innerHeight;

      ctx.drawWindow(
        this.contentWindow,
        0,
        0,
        innerWidth,
        innerHeight,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("NRWebScraperChild: Error taking screenshot:", e);
      return null;
    }
  }

  /**
   * Takes a screenshot of a specific element
   *
   * This method captures a specific element on the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   */
  async takeElementScreenshot(selector: string): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const element = this.document?.querySelector(selector) as HTMLElement;

      if (!element) {
        return null;
      }

      this.applyPersistentEffect(element, "Screenshot");

      const canvas = (await this.contentWindow.document?.createElement(
        "canvas",
      )) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;
      const rect = element.getBoundingClientRect();

      canvas.width = rect.width;
      canvas.height = rect.height;

      ctx.drawWindow(
        this.contentWindow,
        rect.left,
        rect.top,
        rect.width,
        rect.height,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("NRWebScraperChild: Error taking element screenshot:", e);
      return null;
    }
  }

  /**
   * Takes a screenshot of the full page
   *
   * This method captures the entire scrollable area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   */
  async takeFullPageScreenshot(): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const doc = this.contentWindow.document;
      const canvas = (await doc?.createElement("canvas")) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

      const width = doc?.documentElement?.scrollWidth ?? 0;
      const height = doc?.documentElement?.scrollHeight ?? 0;

      canvas.width = width;
      canvas.height = height;

      ctx.drawWindow(
        this.contentWindow,
        0, // x
        0, // y
        width,
        height,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("NRWebScraperChild: Error taking full page screenshot:", e);
      return null;
    }
  }

  /**
   * Takes a screenshot of a specific region of the page.
   * If properties are omitted, they default to the maximum possible size.
   *
   * @param rect The rectangular area to capture { x, y, width, height }.
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   */
  async takeRegionScreenshot(rect: {
    x?: number;
    y?: number;
    width?: number;
    height?: number;
  }): Promise<string | null> {
    try {
      if (!this.contentWindow) {
        return null;
      }
      const doc = this.contentWindow.document;
      const canvas = (await doc?.createElement("canvas")) as HTMLCanvasElement;
      const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

      const pageScrollWidth = doc?.documentElement?.scrollWidth ?? 0;
      const pageScrollHeight = doc?.documentElement?.scrollHeight ?? 0;

      const x = rect.x ?? 0;
      const y = rect.y ?? 0;
      const width = rect.width ?? pageScrollWidth - x;
      const height = rect.height ?? pageScrollHeight - y;

      // Ensure the capture area does not exceed the page dimensions
      const captureX = Math.max(0, x);
      const captureY = Math.max(0, y);
      const captureWidth = Math.min(width, pageScrollWidth - captureX);
      const captureHeight = Math.min(height, pageScrollHeight - captureY);

      canvas.width = captureWidth;
      canvas.height = captureHeight;

      ctx.drawWindow(
        this.contentWindow,
        captureX,
        captureY,
        captureWidth,
        captureHeight,
        "rgb(255,255,255)",
      );

      return canvas.toDataURL("image/png");
    } catch (e) {
      console.error("NRWebScraperChild: Error taking region screenshot:", e);
      return null;
    }
  }

  /**
   * Fills multiple form fields based on a selector-value map.
   *
   * @param formData A map where keys are CSS selectors for input fields
   * and values are the corresponding values to set.
   * @returns boolean - True if all fields were filled successfully, false otherwise.
   */
  async fillForm(
    formData: { [selector: string]: string },
    highlight?: HighlightRequest,
  ): Promise<boolean> {
    try {
      let allFilled = true;
      const selectors = Object.keys(formData);
      const fieldCount = selectors.length;

      for (let i = 0; i < selectors.length; i++) {
        const selector = selectors[i];
        if (!Object.prototype.hasOwnProperty.call(formData, selector)) {
          continue;
        }

        const value = formData[selector];
        const element = this.document?.querySelector(selector) as
          | HTMLInputElement
          | HTMLTextAreaElement
          | null;

        if (element) {
          const elementInfo = `Filling form field ${i + 1}/${fieldCount}: "${value.substring(0, 30)}${value.length > 30 ? "..." : ""}"`;
          await this.applyHighlight(element, highlight, elementInfo);

          element.value = value;
          element.dispatchEvent(new Event("input", { bubbles: true }));
          element.dispatchEvent(new Event("change", { bubbles: true }));
          element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));
          this.applyPersistentEffect(
            element,
            highlight?.action ?? `Fill ${selector}`,
          );
        } else {
          console.warn(
            `NRWebScraperChild: Element not found for selector: ${selector}`,
          );
          allFilled = false;
        }
      }
      return allFilled;
    } catch (e) {
      console.error("NRWebScraperChild: Error filling form:", e);
      return false;
    }
  }

  /**
   * Gets the value of an input or textarea element by CSS selector
   * @param selector - CSS selector to find the target element
   * @returns string | null - The current value, or null if not found/unsupported
   */
  getValue(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (element && typeof (element as HTMLInputElement).value === "string") {
        this.applyPersistentEffect(element, "Read");
        return (element as HTMLInputElement).value;
      }
      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting value:", e);
      return null;
    }
  }

  /**
   * Submits a form. If selector points to an element inside a form, submits its closest form.
   * If selector is a form element, submits that form.
   */
  async submit(
    selector: string,
    highlight?: HighlightRequest,
  ): Promise<boolean> {
    try {
      const root = this.document?.querySelector(selector) as Element | null;
      const form =
        (root as HTMLFormElement | null)?.tagName === "FORM"
          ? (root as HTMLFormElement)
          : (root?.closest?.("form") as HTMLFormElement | null);

      if (!form) return false;

      const formName =
        form.getAttribute("name") || form.getAttribute("id") || "form";
      const elementInfo = `Submitting form: ${formName}`;

      await this.applyHighlight(root ?? form, highlight, elementInfo);

      try {
        // Prefer requestSubmit if available, otherwise fallback to submit.
        const maybeRequestSubmit = (
          form as HTMLFormElement & {
            requestSubmit?: () => void;
          }
        ).requestSubmit;
        if (typeof maybeRequestSubmit === "function") {
          maybeRequestSubmit.call(form);
        } else {
          form.submit();
        }
      } catch (e) {
        // If anything goes wrong, try a plain submit and swallow errors.
        void e;
        try {
          form.submit();
        } catch (e2) {
          void e2;
        }
      }
      this.applyPersistentEffect(form, highlight?.action ?? "Submit");
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error submitting form:", e);
      return false;
    }
  }
}
