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
  private pendingPersistentEffect: {
    element: Element;
    action?: string;
  } | null = null;

  private normalizeHighlightOptions(
    highlight?: HighlightRequest,
  ): NormalizedHighlightOptions | null {
    if (!highlight) {
      return null;
    }

    return {
      action: highlight.action,
      duration: Math.max(highlight.duration ?? 1800, 300),
      focus: highlight.focus ?? true,
      scrollBehavior: highlight.scrollBehavior ?? "smooth",
      padding: Math.max(highlight.padding ?? 14, 0),
      delay: Math.max(highlight.delay ?? 400, 0),
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
        } catch (_) {
          // ignore cleanup errors
        }
      }
      this.highlightCleanupCallbacks = [];

      // 保留中の永続エフェクトを適用
      if (this.pendingPersistentEffect) {
        const { element, action } = this.pendingPersistentEffect;
        this.pendingPersistentEffect = null;
        // 少し遅延してから永続エフェクトを適用（スムーズな遷移のため）
        if (win) {
          win.setTimeout(() => {
            this.applyPersistentEffect(element, action);
          }, 100);
        } else {
          this.applyPersistentEffect(element, action);
        }
      }

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

  private getActionIcon(action: string): string {
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

  private showInfoPanel(
    action: string,
    selector?: string,
    elementInfo?: string,
    elementCount?: number,
    currentProgress?: number,
    totalProgress?: number,
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

      const icon = doc.createElement("span");
      icon.className = "nr-webscraper-info-panel__icon";
      icon.textContent = this.getActionIcon(action);

      const badge = doc.createElement("span");
      badge.className = "nr-webscraper-info-panel__badge";
      badge.textContent = action;

      title.append(icon);
      title.append(badge);
      title.append(doc.createTextNode("操作中"));

      const content = doc.createElement("div");
      content.className = "nr-webscraper-info-panel__content";

      if (elementInfo) {
        content.textContent = elementInfo;
      } else {
        content.textContent = `${action} 操作を実行中...`;
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
        progressBar.style.setProperty("width", `${progressPercent}%`);
        progressContainer.append(progressBar);
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
        countDiv.textContent = `📊 ${elementCount} 個の要素に影響`;
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
  outline: 4px solid rgba(37, 99, 235, 0.9);
  outline-offset: 3px;
  box-shadow: 0 0 0 2px rgba(37, 99, 235, 0.3),
              0 0 20px rgba(37, 99, 235, 0.4),
              inset 0 0 15px rgba(37, 99, 235, 0.1);
  transition: outline-color 200ms ease-out, box-shadow 200ms ease-out, opacity 300ms ease-in;
  position: relative;
  animation: nr-webscraper-effect-pulse 2s ease-in-out infinite,
             nr-webscraper-effect-fade-in 300ms ease-in;
  opacity: 0;
}

@keyframes nr-webscraper-effect-fade-in {
  from {
    opacity: 0;
    transform: scale(0.98);
  }
  to {
    opacity: 1;
    transform: scale(1);
  }
}

.nr-webscraper-effect.nr-webscraper-effect--visible {
  opacity: 1;
}

@keyframes nr-webscraper-effect-pulse {
  0%, 100% {
    box-shadow: 0 0 0 2px rgba(37, 99, 235, 0.3),
                0 0 20px rgba(37, 99, 235, 0.4),
                inset 0 0 15px rgba(37, 99, 235, 0.1);
  }
  50% {
    box-shadow: 0 0 0 4px rgba(37, 99, 235, 0.4),
                0 0 30px rgba(37, 99, 235, 0.6),
                inset 0 0 20px rgba(37, 99, 235, 0.15);
  }
}

.nr-webscraper-effect[data-nr-webscraper-effect]:not([data-nr-webscraper-effect=""])::after {
  content: attr(data-nr-webscraper-effect);
  position: absolute;
  top: -32px;
  left: 0;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: rgba(37, 99, 235, 0.95);
  color: #fff;
  padding: 5px 12px;
  border-radius: 9999px;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 12px;
  font-weight: 700;
  white-space: nowrap;
  pointer-events: none;
  transform: translateY(-4px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3),
              0 0 20px rgba(37, 99, 235, 0.5);
  z-index: 2147483646;
  text-transform: uppercase;
  letter-spacing: 0.5px;
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

      // 既に同じ要素にエフェクトが適用されている場合はスキップ
      if (this.persistentEffects.has(element)) {
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

      // フェードインアニメーションをトリガー
      const win = this.contentWindow;
      if (win) {
        win.requestAnimationFrame(() => {
          element.classList.add("nr-webscraper-effect--visible");
        });
      } else {
        element.classList.add("nr-webscraper-effect--visible");
      }
    } catch (error) {
      console.warn(
        "NRWebScraperChild: Failed to apply persistent effect",
        error,
      );
    }
  }

  private clearPersistentEffects(options?: { keepStyle?: boolean }): void {
    const keepStyle = options?.keepStyle ?? false;

    // 保留中の永続エフェクトをクリア
    this.pendingPersistentEffect = null;

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

    // Show info panel with progress
    this.showInfoPanel(
      options.action ?? "Highlight",
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
    onCleanup?: (element: Element) => void,
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

    // クリーンアップ時に永続エフェクトを適用するように設定
    if (onCleanup && target) {
      this.pendingPersistentEffect = {
        element: target,
        action: options.action,
      };
    }

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
      case "WebScraper:ClearEffects":
        this.clearPersistentEffects();
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

      const elementInfo = `入力値を設定: "${value.substring(0, 50)}${value.length > 50 ? "..." : ""}"`;

      // 情報パネルを表示
      this.showInfoPanel(
        highlight?.action ?? "Input",
        undefined,
        elementInfo,
        1,
      );

      // 一時ハイライトを表示（クリーンアップ時に永続エフェクトを適用）
      await this.applyHighlight(element, highlight, elementInfo, () => {
        // クリーンアップ時に永続エフェクトを適用するコールバック
        // 実際の適用は cleanupHighlight 内で行われる
      });

      element.value = value;
      // Trigger input event to ensure the change is detected
      element.dispatchEvent(new Event("input", { bubbles: true }));
      // Some sites update bound state on change/blur
      element.dispatchEvent(new Event("change", { bubbles: true }));
      element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));
      // 永続エフェクトは一時ハイライトのクリーンアップ時に自動適用される
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
        ? `${elementTagName} をクリック: "${elementText}${elementText.length >= 30 ? "..." : ""}"`
        : `${elementTagName} 要素をクリック中`;

      // 情報パネルを表示
      this.showInfoPanel(
        highlight?.action ?? "Click",
        undefined,
        elementInfo,
        1,
      );

      // 一時ハイライトを表示（クリーンアップ時に永続エフェクトを適用）
      await this.applyHighlight(element, highlight, elementInfo, () => {
        // クリーンアップ時に永続エフェクトを適用するコールバック
      });

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
      // 永続エフェクトは一時ハイライトのクリーンアップ時に自動適用される
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
      const win = this.contentWindow;
      const doc = this.document;

      // 初期情報パネルを表示
      if (fieldCount > 1 && doc) {
        this.showInfoPanel(
          highlight?.action ?? "Fill",
          undefined,
          `フォーム入力中: ${fieldCount} 個のフィールド`,
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
          const elementInfo = `フィールド ${i + 1}/${fieldCount}: "${value.substring(0, 30)}${value.length > 30 ? "..." : ""}"`;

          // 進捗を更新
          if (fieldCount > 1 && doc) {
            this.showInfoPanel(
              highlight?.action ?? "Fill",
              undefined,
              elementInfo,
              fieldCount,
              i + 1,
              fieldCount,
            );
          }

          // 一時ハイライトを表示（クリーンアップ時に永続エフェクトを適用）
          await this.applyHighlight(element, highlight, elementInfo, () => {
            // クリーンアップ時に永続エフェクトを適用するコールバック
          });

          element.value = value;
          element.dispatchEvent(new Event("input", { bubbles: true }));
          element.dispatchEvent(new Event("change", { bubbles: true }));
          element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));
          // 永続エフェクトは一時ハイライトのクリーンアップ時に自動適用される

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
      const elementInfo = `フォーム送信中: ${formName}`;

      // 情報パネルを表示
      this.showInfoPanel(
        highlight?.action ?? "Submit",
        undefined,
        elementInfo,
        1,
      );

      // 一時ハイライトを表示（クリーンアップ時に永続エフェクトを適用）
      await this.applyHighlight(root ?? form, highlight, elementInfo, () => {
        // クリーンアップ時に永続エフェクトを適用するコールバック
      });

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
      // 永続エフェクトは一時ハイライトのクリーンアップ時に自動適用される
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error submitting form:", e);
      return false;
    }
  }
}
