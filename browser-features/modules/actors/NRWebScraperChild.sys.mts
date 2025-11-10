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

interface NRWebScraperMessageData {
  selector?: string;
  value?: string;
  textContent?: string;
  timeout?: number;
  script?: string;
  rect?: { x?: number; y?: number; width?: number; height?: number };
  formData?: { [selector: string]: string };
  elementInfo?: string;
}

interface NormalizedHighlightOptions {
  action: string;
  duration: number;
  focus: boolean;
  scrollBehavior: HighlightScrollBehavior;
  padding: number;
  delay: number;
}

type HighlightOptionsInput = Partial<NormalizedHighlightOptions> & {
  action?: string;
};

type HighlightFocusOptions = {
  preventScroll?: boolean;
};

const HIGHLIGHT_PRESETS: Record<string, HighlightOptionsInput> = {
  Highlight: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 400,
  },
  Inspect: {
    duration: 1800,
    focus: false,
    scrollBehavior: "smooth",
    padding: 16,
    delay: 400,
  },
  Input: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 400,
  },
  Click: {
    duration: 2000,
    focus: true,
    scrollBehavior: "smooth",
    padding: 12,
    delay: 500,
  },
  Fill: {
    duration: 1500,
    focus: true,
    scrollBehavior: "smooth",
    padding: 18,
    delay: 500,
  },
  Submit: {
    duration: 1800,
    focus: true,
    scrollBehavior: "smooth",
    padding: 12,
    delay: 500,
  },
};

export class NRWebScraperChild extends JSWindowActorChild {
  private highlightOverlay: HTMLDivElement | null = null;
  private highlightOverlays: HTMLDivElement[] = [];
  private highlightCleanupTimer: number | null = null;
  private highlightCleanupCallbacks: Array<() => void> = [];
  private highlightStyleElement: HTMLStyleElement | null = null;
  private infoPanel: HTMLDivElement | null = null;
  private infoPanelCleanupTimer: number | null = null;

  private async translate(
    key: string,
    vars: Record<string, string | number> = {},
  ): Promise<string> {
    // Try to get translation from parent process via message
    try {
      const result = (await this.sendQuery("WebScraper:Translate", {
        key: `browser-chrome:nr-webscraper.${key}`,
        vars,
      })) as string | null | undefined;

      if (result && typeof result === "string" && result.trim().length > 0) {
        return this.formatTemplate(result, vars);
      }
    } catch (error) {
      // Silently fallback to English on translation errors
      void error;
    }
    // Fallback to English
    return this.formatTemplate(this.getFallbackString(key), vars);
  }

  private formatTemplate(
    template: string,
    vars: Record<string, string | number>,
  ): string {
    return template
      .replace(/{{\s*(\w+)\s*}}/g, (_match, name) => {
        const value = vars[name];
        return value !== undefined && value !== null ? String(value) : "";
      })
      .replace(/{\s*(\w+)\s*}/g, (_match, name) => {
        const value = vars[name];
        return value !== undefined && value !== null ? String(value) : "";
      });
  }

  private getFallbackString(key: string): string {
    const fallbacks: Record<string, string> = {
      panelTitle: "Floorp OS is operating",
      panelDefaultMessage: "{{action}} in progress...",
      panelProgressSummary: "Affects {{count}} element(s)",
      formSummary: "Filling form: {{count}} field(s)",
      formFieldProgress: 'Field {{current}}/{{total}}: "{{value}}"',
      inputValueSet: 'Set input value: "{{value}}"',
      clickElementWithText: 'Click {{tag}}: "{{text}}"',
      clickElementNoText: "Clicking {{tag}} element",
      inspectPageHtml: "Retrieved full page HTML",
      inspectGetElement: "Retrieved element: {{selector}}",
      inspectGetElements: "{{count}} element(s) matched: {{selector}}",
      inspectGetElementByText: 'Retrieved element matching text: "{{text}}"',
      inspectGetElementText: "Retrieved text from: {{selector}}",
      inspectGetValue: "Retrieved value: {{selector}}",
      submitForm: "Submitting form: {{name}}",
      actionLabelHighlight: "Highlight",
      actionLabelInspect: "Inspect",
      actionLabelFill: "Fill",
      actionLabelInput: "Input",
      actionLabelClick: "Click",
      actionLabelSubmit: "Submit",
    };
    return fallbacks[key] || key;
  }

  private async getActionLabel(action: string | undefined): Promise<string> {
    if (!action) {
      return await this.translate("actionLabelHighlight");
    }
    const actionMap: Record<string, string> = {
      highlight: "actionLabelHighlight",
      inspect: "actionLabelInspect",
      fill: "actionLabelFill",
      input: "actionLabelInput",
      click: "actionLabelClick",
      submit: "actionLabelSubmit",
    };
    const key = actionMap[action.toLowerCase()];
    if (key) {
      return await this.translate(key);
    }
    return action;
  }

  private truncate(value: string, limit: number): string {
    if (value.length <= limit) {
      return value;
    }
    return `${value.substring(0, limit)}...`;
  }

  /**
   * Helper method to run async inspection highlighting without blocking
   * synchronous return values. Used for information-gathering APIs.
   */
  private runAsyncInspection(
    target: Element | Element[],
    translationKey: string,
    translationVars: Record<string, string | number> = {},
  ): void {
    void (async () => {
      try {
        const elementInfo = await this.translate(
          translationKey,
          translationVars,
        );
        await this.highlightInspection(target, elementInfo);
      } catch {
        // Silently ignore inspection errors
      }
    })();
  }

  private getHighlightOptions(
    action: string,
    overrides: HighlightOptionsInput = {},
  ): HighlightOptionsInput {
    const base = HIGHLIGHT_PRESETS[action] ?? HIGHLIGHT_PRESETS.Highlight;
    return { ...base, ...overrides, action };
  }

  private normalizeHighlightOptions(
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
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-45),
                0 0 20px var(--nr-highlight-color-alpha-25);
    transform: scale(1);
  }
  50% {
    box-shadow: 0 0 0 12px var(--nr-highlight-color-alpha-25),
                0 0 40px var(--nr-highlight-color-alpha-45);
    transform: scale(1.02);
  }
  100% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-0),
                0 0 20px var(--nr-highlight-color-alpha-25);
    transform: scale(1);
  }
}

@keyframes nr-webscraper-highlight-glow {
  0%, 100% {
    box-shadow: 0 0 0 0 var(--nr-highlight-color-alpha-45),
                0 0 20px var(--nr-highlight-color-alpha-25),
                inset 0 0 20px var(--nr-highlight-color-alpha-15);
  }
  50% {
    box-shadow: 0 0 0 8px var(--nr-highlight-color-alpha-30),
                0 0 50px var(--nr-highlight-color-alpha-50),
                inset 0 0 30px var(--nr-highlight-color-alpha-20);
  }
}

@keyframes nr-webscraper-info-slide-in {
  from {
    opacity: 0;
    transform: translateY(-20px) scale(0.95);
  }
  to {
    opacity: 1;
    transform: translateY(0) scale(1);
  }
}

@keyframes nr-webscraper-progress {
  from {
    width: 0%;
  }
  to {
    width: 100%;
  }
}

.nr-webscraper-highlight-overlay {
  --nr-highlight-color-alpha-0: rgba(59, 130, 246, 0);
  --nr-highlight-color-alpha-15: rgba(59, 130, 246, 0.15);
  --nr-highlight-color-alpha-20: rgba(59, 130, 246, 0.20);
  --nr-highlight-color-alpha-25: rgba(59, 130, 246, 0.25);
  --nr-highlight-color-alpha-30: rgba(59, 130, 246, 0.30);
  --nr-highlight-color-alpha-45: rgba(59, 130, 246, 0.45);
  --nr-highlight-color-alpha-50: rgba(59, 130, 246, 0.50);
  --nr-highlight-color: rgba(59, 130, 246, 0.95);
  --nr-highlight-bg: rgba(59, 130, 246, 0.12);
  --nr-label-bg: rgba(37, 99, 235, 0.94);
  position: fixed;
  border-radius: 12px;
  border: 3px solid var(--nr-highlight-color);
  box-shadow: 0 0 0 6px var(--nr-highlight-color-alpha-25),
              0 0 30px var(--nr-highlight-color-alpha-30);
  pointer-events: none;
  z-index: 2147483645;
  opacity: 0;
  transform: scale(0.96);
  transition: opacity 150ms ease-out, transform 150ms ease-out;
  background: var(--nr-highlight-bg);
  animation: nr-webscraper-highlight-glow 2s ease-in-out infinite;
}

.nr-webscraper-highlight-overlay--read {
  --nr-highlight-color-alpha-0: rgba(34, 197, 94, 0);
  --nr-highlight-color-alpha-15: rgba(34, 197, 94, 0.15);
  --nr-highlight-color-alpha-20: rgba(34, 197, 94, 0.20);
  --nr-highlight-color-alpha-25: rgba(34, 197, 94, 0.25);
  --nr-highlight-color-alpha-30: rgba(34, 197, 94, 0.30);
  --nr-highlight-color-alpha-45: rgba(34, 197, 94, 0.45);
  --nr-highlight-color-alpha-50: rgba(34, 197, 94, 0.50);
  --nr-highlight-color: rgba(34, 197, 94, 0.95);
  --nr-highlight-bg: rgba(34, 197, 94, 0.12);
  --nr-label-bg: rgba(22, 163, 74, 0.94);
}

.nr-webscraper-highlight-overlay--write {
  --nr-highlight-color-alpha-0: rgba(168, 85, 247, 0);
  --nr-highlight-color-alpha-15: rgba(168, 85, 247, 0.15);
  --nr-highlight-color-alpha-20: rgba(168, 85, 247, 0.20);
  --nr-highlight-color-alpha-25: rgba(168, 85, 247, 0.25);
  --nr-highlight-color-alpha-30: rgba(168, 85, 247, 0.30);
  --nr-highlight-color-alpha-45: rgba(168, 85, 247, 0.45);
  --nr-highlight-color-alpha-50: rgba(168, 85, 247, 0.50);
  --nr-highlight-color: rgba(168, 85, 247, 0.95);
  --nr-highlight-bg: rgba(168, 85, 247, 0.12);
  --nr-label-bg: rgba(147, 51, 234, 0.94);
}

.nr-webscraper-highlight-overlay--click {
  --nr-highlight-color-alpha-0: rgba(249, 115, 22, 0);
  --nr-highlight-color-alpha-15: rgba(249, 115, 22, 0.15);
  --nr-highlight-color-alpha-20: rgba(249, 115, 22, 0.20);
  --nr-highlight-color-alpha-25: rgba(249, 115, 22, 0.25);
  --nr-highlight-color-alpha-30: rgba(249, 115, 22, 0.30);
  --nr-highlight-color-alpha-45: rgba(249, 115, 22, 0.45);
  --nr-highlight-color-alpha-50: rgba(249, 115, 22, 0.50);
  --nr-highlight-color: rgba(249, 115, 22, 0.95);
  --nr-highlight-bg: rgba(249, 115, 22, 0.12);
  --nr-label-bg: rgba(234, 88, 12, 0.94);
}

.nr-webscraper-highlight-overlay--submit {
  --nr-highlight-color-alpha-0: rgba(239, 68, 68, 0);
  --nr-highlight-color-alpha-15: rgba(239, 68, 68, 0.15);
  --nr-highlight-color-alpha-20: rgba(239, 68, 68, 0.20);
  --nr-highlight-color-alpha-25: rgba(239, 68, 68, 0.25);
  --nr-highlight-color-alpha-30: rgba(239, 68, 68, 0.30);
  --nr-highlight-color-alpha-45: rgba(239, 68, 68, 0.45);
  --nr-highlight-color-alpha-50: rgba(239, 68, 68, 0.50);
  --nr-highlight-color: rgba(239, 68, 68, 0.95);
  --nr-highlight-bg: rgba(239, 68, 68, 0.12);
  --nr-label-bg: rgba(220, 38, 38, 0.94);
}

.nr-webscraper-highlight-overlay.nr-webscraper-highlight-overlay--visible {
  opacity: 1;
  transform: scale(1);
  animation: nr-webscraper-highlight-pulse 1200ms ease-in-out 1,
             nr-webscraper-highlight-glow 2s ease-in-out infinite;
  transition: opacity 300ms ease-out, transform 300ms ease-out;
}

.nr-webscraper-highlight-label {
  position: absolute;
  top: -32px;
  left: 0;
  transform: translateY(-4px);
  background: var(--nr-label-bg);
  color: #fff;
  padding: 6px 12px;
  border-radius: 9999px;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 13px;
  font-weight: 700;
  white-space: nowrap;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3),
              0 0 20px var(--nr-highlight-color-alpha-50);
  letter-spacing: 0.3px;
  text-transform: uppercase;
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
  max-width: 420px;
  min-width: 320px;
  background: rgba(17, 24, 39, 0.98);
  backdrop-filter: blur(16px);
  border: 2px solid rgba(255, 255, 255, 0.15);
  border-radius: 16px;
  box-shadow: 0 25px 60px rgba(0, 0, 0, 0.4),
              0 0 0 1px rgba(255, 255, 255, 0.08),
              inset 0 1px 0 rgba(255, 255, 255, 0.1);
  pointer-events: none;
  z-index: 2147483647;
  animation: nr-webscraper-info-slide-in 300ms cubic-bezier(0.34, 1.56, 0.64, 1);
  color: #fff;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 13px;
  padding: 18px 20px;
}

.nr-webscraper-info-panel__title {
  font-weight: 700;
  font-size: 15px;
  margin-bottom: 10px;
  display: flex;
  align-items: center;
  gap: 10px;
  color: #fff;
}

.nr-webscraper-info-panel__icon {
  width: 20px;
  height: 20px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  line-height: 1;
}

.nr-webscraper-info-panel__badge {
  display: inline-flex;
  align-items: center;
  padding: 4px 10px;
  border-radius: 9999px;
  font-size: 11px;
  font-weight: 700;
  background: var(--nr-label-bg);
  color: #fff;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.2);
}

.nr-webscraper-info-panel__content {
  color: rgba(255, 255, 255, 0.9);
  line-height: 1.6;
  margin-bottom: 8px;
}

.nr-webscraper-info-panel__progress {
  margin-top: 12px;
  height: 4px;
  background: rgba(255, 255, 255, 0.1);
  border-radius: 2px;
  overflow: hidden;
  position: relative;
}

.nr-webscraper-info-panel__progress-bar {
  height: 100%;
  background: linear-gradient(90deg, var(--nr-label-bg), var(--nr-highlight-color));
  border-radius: 2px;
  animation: nr-webscraper-progress 0.3s ease-out;
  box-shadow: 0 0 10px var(--nr-highlight-color-alpha-50);
}

.nr-webscraper-info-panel__selector {
  margin-top: 10px;
  padding: 10px 12px;
  background: rgba(0, 0, 0, 0.4);
  border-radius: 8px;
  font-family: "SF Mono", Monaco, Consolas, monospace;
  font-size: 11px;
  color: rgba(168, 85, 247, 0.95);
  word-break: break-all;
  border: 1px solid rgba(255, 255, 255, 0.08);
  line-height: 1.4;
}

.nr-webscraper-info-panel__count {
  margin-top: 8px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.7);
  font-weight: 500;
  display: flex;
  align-items: center;
  gap: 6px;
}`;

    (doc.head ?? doc.documentElement)?.append(style);
    this.highlightStyleElement = style;
  }

  private async highlightInspection(
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
      // エフェクト表示に失敗しても処理には影響させない
    }
  }

  private cleanupHighlight(): void {
    const win = this.contentWindow;

    if (this.highlightCleanupTimer !== null && win) {
      win.clearTimeout(this.highlightCleanupTimer);
    }
    this.highlightCleanupTimer = null;

    // 一時ハイライトをフェードアウト
    const fadeOutAndCleanup = () => {
      const overlaysToRemove = [
        ...(this.highlightOverlay ? [this.highlightOverlay] : []),
        ...this.highlightOverlays,
      ];

      if (overlaysToRemove.length > 0) {
        overlaysToRemove.forEach((overlay) => {
          if (overlay?.isConnected) {
            overlay.style.setProperty(
              "transition",
              "opacity 300ms ease-out, transform 300ms ease-out",
            );
            overlay.style.setProperty("opacity", "0");
            overlay.style.setProperty("transform", "scale(0.98)");
            win?.setTimeout(() => {
              if (overlay?.isConnected) {
                overlay.remove();
              }
            }, 300);
          }
        });
      }

      // クリーンアップコールバックを実行
      for (const cleanup of this.highlightCleanupCallbacks) {
        try {
          cleanup();
        } catch {
          // ignore cleanup errors
        }
      }
      this.highlightCleanupCallbacks = [];

      this.highlightOverlay = null;
      this.highlightOverlays = [];
    };

    fadeOutAndCleanup();
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

  /**
   * Lucide icon SVG path data mapping
   * These are the SVG path strings from Lucide icons
   */
  private readonly LUCIDE_ICON_PATHS: Record<string, string> = {
    Eye: "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z",
    Pencil:
      "M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7 M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z",
    MousePointerClick:
      "M9 9l5 12 1.774-5.226L21 14 9 9z M16.071 16.071l4.243 4.243 M7.188 2.239l.777 2.897M5.136 7.965l-2.898-.777M13.95 4.05l-2.122 2.122m-5.657 5.656l-2.12 2.122",
    Send: "M22 2L11 13 M22 2l-7 20-4-9-9-4z",
    Zap: "M13 2L3 14h9l-1 8 10-12h-9l1-8z",
  };

  /**
   * Creates an SVG icon element using Lucide icon paths
   */
  private createLucideIcon(
    doc: Document,
    iconName: keyof typeof this.LUCIDE_ICON_PATHS,
    size = 20,
  ): Element {
    const svgString = `<svg width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" xmlns="http://www.w3.org/2000/svg"><path d="${this.LUCIDE_ICON_PATHS[iconName] || this.LUCIDE_ICON_PATHS.Zap}"/></svg>`;
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

  private getActionIcon(action: string): Element | string {
    const doc = this.document;
    if (!doc) {
      // Fallback to emoji if document is not available
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

  private getActionIconEmoji(action: string): string {
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

  private async showInfoPanel(
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
        // Fallback to emoji if SVG creation failed
        icon.textContent = this.getActionIconEmoji(action);
      }

      const badge = doc.createElement("span");
      badge.className = "nr-webscraper-info-panel__badge";
      badge.textContent = await this.getActionLabel(action);

      title.append(icon);
      title.append(badge);
      title.append(doc.createTextNode(await this.translate("panelTitle")));

      const content = doc.createElement("div");
      content.className = "nr-webscraper-info-panel__content";

      if (elementInfo) {
        content.textContent = elementInfo;
      } else {
        content.textContent = await this.translate("panelDefaultMessage", {
          action: await this.getActionLabel(action),
        });
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
        countDiv.textContent = await this.translate("panelProgressSummary", {
          count: elementCount,
        });
        panel.append(countDiv);
      }

      (doc.body ?? doc.documentElement)?.append(panel);
      this.infoPanel = panel;

      // Auto-hide after action completes (長めに設定)
      this.infoPanelCleanupTimer = Number(
        win.setTimeout(() => this.hideInfoPanel(), 4000),
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

  private async applyHighlightMultiple(
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
    this.cleanupHighlight();

    const colorClass = this.getActionColorClass(options.action);

    // Show info panel with progress
    await this.showInfoPanel(
      options.action,
      undefined,
      elementInfo,
      targets.length,
      0,
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
      } catch {
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
    optionsInput: HighlightOptionsInput = {},
    elementInfo?: string,
    showPanel = true,
  ): Promise<boolean> {
    if (!target) {
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
    this.cleanupHighlight();

    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-highlight-overlay";

    const colorClass = this.getActionColorClass(options.action);
    if (colorClass) {
      overlay.classList.add(colorClass);
    }

    // Show info panel
    if (showPanel) {
      await this.showInfoPanel(options.action, undefined, elementInfo, 1);
    }

    const padding = options.padding;

    if (options.action) {
      const label = doc.createElement("div");
      label.className = "nr-webscraper-highlight-label";
      label.textContent = await this.getActionLabel(options.action);
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
        element.focus({ preventScroll: true } as HighlightFocusOptions);
      } catch {
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
          return this.inputElement(message.data.selector, message.data.value);
        }
        break;
      case "WebScraper:ClickElement":
        if (message.data?.selector) {
          return this.clickElement(message.data.selector);
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
          return this.fillForm(message.data.formData);
        }
        break;
      case "WebScraper:Submit":
        if (message.data?.selector) {
          return this.submit(message.data.selector);
        }
        break;
      case "WebScraper:ClearEffects":
        this.cleanupHighlight();
        this.hideInfoPanel();
        return true;
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

        const docElement = this.contentWindow.document?.documentElement;
        if (docElement) {
          this.runAsyncInspection(docElement, "inspectPageHtml");
        }

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
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.runAsyncInspection(element, "inspectGetElement", { selector });
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
      nodeList.forEach((el: Element) => {
        const o = el.outerHTML;
        if (o != null) results.push(String(o));
      });
      const elements = Array.from(nodeList) as Element[];
      if (elements.length > 0) {
        this.runAsyncInspection(elements, "inspectGetElements", {
          selector,
          count: elements.length,
        });
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
          this.runAsyncInspection(el, "inspectGetElementByText", {
            text: this.truncate(textContent, 30),
          });
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
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.runAsyncInspection(element, "inspectGetElementText", { selector });
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
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.runAsyncInspection(element, "inspectGetElementText", { selector });
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
  async inputElement(selector: string, value: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (!element) {
        return false;
      }

      const truncatedValue = this.truncate(value, 50);
      const elementInfo = await this.translate("inputValueSet", {
        value: truncatedValue,
      });
      const options = this.getHighlightOptions("Input");

      await this.applyHighlight(element, options, elementInfo);

      element.value = value;
      // Trigger input event to ensure the change is detected
      element.dispatchEvent(new Event("input", { bubbles: true }));
      element.dispatchEvent(new Event("change", { bubbles: true }));
      element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));
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
  async clickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return Promise.resolve(false);

      const elementTagName = element.tagName?.toLowerCase() || "element";
      const elementTextRaw = element.textContent?.trim() || "";
      const truncatedText = this.truncate(elementTextRaw, 30);
      const elementInfo =
        elementTextRaw.length > 0
          ? await this.translate("clickElementWithText", {
              tag: elementTagName,
              text: truncatedText,
            })
          : await this.translate("clickElementNoText", { tag: elementTagName });
      const options = this.getHighlightOptions("Click");

      // エフェクト表示を非同期で開始（クリック処理をブロックしない）
      const effectPromise = this.applyHighlight(
        element,
        options,
        elementInfo,
      ).catch(() => {
        // エフェクト表示のエラーは無視（クリック処理には影響しない）
      });

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

      // エフェクト表示の完了を待たずに結果を返す（非同期で継続）
      void effectPromise;

      return Promise.resolve(success);
    } catch (e) {
      console.error("NRWebScraperChild: Error clicking element:", e);
      return Promise.resolve(false);
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
  async fillForm(formData: { [selector: string]: string }): Promise<boolean> {
    try {
      let allFilled = true;
      const selectors = Object.keys(formData);
      const fieldCount = selectors.length;
      const win = this.contentWindow;
      const doc = this.document;
      const action = "Fill";
      const highlightOptions = this.getHighlightOptions(action);

      // 初期情報パネルを表示
      if (fieldCount > 1 && doc) {
        await this.showInfoPanel(
          action,
          undefined,
          await this.translate("formSummary", { count: fieldCount }),
          fieldCount,
          0,
          fieldCount,
        );
      }

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
          const elementInfo = await this.translate("formFieldProgress", {
            current: i + 1,
            total: fieldCount,
            value: this.truncate(value, 30),
          });
          if (fieldCount > 1 && doc) {
            this.showInfoPanel(
              action,
              undefined,
              elementInfo,
              fieldCount,
              i + 1,
              fieldCount,
            );
          }

          await this.applyHighlight(
            element,
            highlightOptions,
            elementInfo,
            false,
          );

          element.value = value;
          element.dispatchEvent(new Event("input", { bubbles: true }));
          element.dispatchEvent(new Event("change", { bubbles: true }));
          element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));

          // 各フィールド入力後に1.2秒の遅延（ユーザーがエフェクトを確認できるように）
          if (i < selectors.length - 1 && win) {
            await new Promise<void>((resolve) => {
              win.setTimeout(() => resolve(), 1200);
            });
          }
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
        this.runAsyncInspection(element, "inspectGetValue", { selector });
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
  async submit(selector: string): Promise<boolean> {
    try {
      const root = this.document?.querySelector(selector) as Element | null;
      const form =
        (root as HTMLFormElement | null)?.tagName === "FORM"
          ? (root as HTMLFormElement)
          : (root?.closest?.("form") as HTMLFormElement | null);

      if (!form) return false;

      const formName =
        form.getAttribute("name") || form.getAttribute("id") || "form";
      const elementInfo = await this.translate("submitForm", {
        name: formName,
      });
      const options = this.getHighlightOptions("Submit");

      await this.applyHighlight(root ?? form, options, elementInfo);

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
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error submitting form:", e);
      return false;
    }
  }
}
