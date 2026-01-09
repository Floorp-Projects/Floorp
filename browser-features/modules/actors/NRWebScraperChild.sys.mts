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
  attributeName?: string;
  checked?: boolean;
  optionValue?: string;
  targetSelector?: string;
  innerHTML?: string;
  eventType?: string;
  eventOptions?: { bubbles?: boolean; cancelable?: boolean };
  typingMode?: boolean;
  typingDelayMs?: number;
  filePath?: string;
  key?: string;
  cookieString?: string;
  cookieName?: string;
  cookieValue?: string;
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

const { setTimeout: timerSetTimeout, clearTimeout: timerClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

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
  InspectPeek: {
    duration: 1800,
    focus: false,
    scrollBehavior: "smooth",
    padding: 14,
    delay: 1200,
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
  private pageHideHandler: (() => void) | null = null;
  // 非同期操作の競合防止用 操作ID
  private highlightOperationId = 0;
  // ハイライトの最小表示時間を保証するためのタイムスタンプ
  private highlightStartTime = 0;
  // 最小表示時間（ミリ秒）
  private static readonly MINIMUM_HIGHLIGHT_DURATION = 500;
  // AI制御中のタブをロックするオーバーレイ
  private controlOverlay: HTMLDivElement | null = null;

  /**
   * Ensures the target element is in view (centers in viewport when possible).
   */
  private scrollIntoViewIfNeeded(element: Element): void {
    try {
      if (!element.isConnected) return;
      element.scrollIntoView({
        behavior: "auto",
        block: "center",
        inline: "center",
      });
    } catch {
      // ignore scrolling errors
    }
  }

  /**
   * Focus helper that avoids scroll jumps.
   */
  private focusElementSoft(element: Element): void {
    const win = this.contentWindow;
    if (!win) return;
    try {
      const rawElement = (element as any).wrappedJSObject ?? element;
      const htmlEl = rawElement as unknown as HTMLElement;
      if (typeof htmlEl.focus === "function") {
        htmlEl.focus({ preventScroll: true });
      }
    } catch {
      // ignore focus errors
    }
  }

  /**
   * Dispatches a pointer + mouse click sequence to improve compatibility with UI frameworks.
   */
  private dispatchPointerClickSequence(
    element: HTMLElement,
    clientX: number,
    clientY: number,
    button: number,
  ): boolean {
    const win = (this.contentWindow ?? null) as
      | (Window & typeof globalThis)
      | null;

    // Use wrappedJSObject to access page context if available
    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;

    const PointerEv = (rawWin?.PointerEvent ?? null) as typeof PointerEvent | null;
    const MouseEv = (rawWin?.MouseEvent ?? globalThis.MouseEvent ?? null) as
      | typeof MouseEvent
      | null;
    const view = rawWin ?? undefined;

    const emit = <T extends Event>(ev: T | null) => {
      if (!ev) return false;
      try {
        return rawElement.dispatchEvent(ev);
      } catch {
        return false;
      }
    };

    const buttons = button === 0 ? 1 : button === 1 ? 4 : 2;

    emit(
      PointerEv
        ? new PointerEv("pointerdown", {
            bubbles: true,
            cancelable: true,
            clientX,
            clientY,
            button,
            buttons,
            pointerType: "mouse",
            view,
          })
        : null,
    );
    emit(
      MouseEv
        ? new MouseEv("mousedown", {
            bubbles: true,
            cancelable: true,
            clientX,
            clientY,
            button,
            buttons,
            view,
          })
        : null,
    );
    emit(
      PointerEv
        ? new PointerEv("pointerup", {
            bubbles: true,
            cancelable: true,
            clientX,
            clientY,
            button,
            buttons,
            pointerType: "mouse",
            view,
          })
        : null,
    );
    emit(
      MouseEv
        ? new MouseEv("mouseup", {
            bubbles: true,
            cancelable: true,
            clientX,
            clientY,
            button,
            buttons,
            view,
          })
        : null,
    );

    const clickOk = emit(
      MouseEv
        ? new MouseEv("click", {
            bubbles: true,
            cancelable: true,
            clientX,
            clientY,
            button,
            buttons,
            view,
          })
        : null,
    );

    return clickOk ?? false;
  }

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
    // 開始時点の操作IDを記録
    const startOperationId = this.highlightOperationId;

    void (async () => {
      try {
        // Check if target(s) are still alive before proceeding
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
            return; // DeadObject
          }
        }

        const elementInfo = await this.translate(
          translationKey,
          translationVars,
        );

        // 翻訳完了後、新しい操作が開始されていたら中断
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
   * Lightweight, non-blocking highlight used for value reads.
   * Keeps duration short and avoids info panel/label noise.
   */
  private runAsyncValuePeek(target: Element): void {
    // 開始時点の操作IDを記録
    const startOperationId = this.highlightOperationId;

    void (async () => {
      try {
        if (!target.isConnected) {
          return;
        }

        // 新しい操作が開始されていたら中断
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
  /* CSS変数のデフォルト値を定義（overlayクラスがない場合のフォールバック） */
  --nr-highlight-color-alpha-50: rgba(59, 130, 246, 0.50);
  --nr-highlight-color: rgba(59, 130, 246, 0.95);
  --nr-label-bg: rgba(37, 99, 235, 0.94);
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
  z-index: 2147483647 !important;
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
}

/* InfoPanel用のカラークラス（オーバーレイと同じカラーテーマを適用） */
.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--read {
  --nr-highlight-color-alpha-50: rgba(34, 197, 94, 0.50);
  --nr-highlight-color: rgba(34, 197, 94, 0.95);
  --nr-label-bg: rgba(22, 163, 74, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--write {
  --nr-highlight-color-alpha-50: rgba(168, 85, 247, 0.50);
  --nr-highlight-color: rgba(168, 85, 247, 0.95);
  --nr-label-bg: rgba(147, 51, 234, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--click {
  --nr-highlight-color-alpha-50: rgba(249, 115, 22, 0.50);
  --nr-highlight-color: rgba(249, 115, 22, 0.95);
  --nr-label-bg: rgba(234, 88, 12, 0.94);
}

.nr-webscraper-info-panel.nr-webscraper-highlight-overlay--submit {
  --nr-highlight-color-alpha-50: rgba(239, 68, 68, 0.50);
  --nr-highlight-color: rgba(239, 68, 68, 0.95);
  --nr-label-bg: rgba(220, 38, 38, 0.94);
}

/* AI Control Overlay - blocks user interaction during automation */
@keyframes nr-webscraper-control-pulse {
  0%, 100% {
    border-color: rgba(59, 130, 246, 0.5);
    box-shadow: inset 0 0 60px rgba(59, 130, 246, 0.08),
                0 0 0 4px rgba(59, 130, 246, 0.15);
  }
  50% {
    border-color: rgba(59, 130, 246, 0.8);
    box-shadow: inset 0 0 100px rgba(59, 130, 246, 0.15),
                0 0 0 8px rgba(59, 130, 246, 0.25);
  }
}

.nr-webscraper-control-overlay {
  position: fixed !important;
  inset: 0 !important;
  z-index: 2147483640 !important;
  background: radial-gradient(
    circle at center,
    transparent 0%,
    transparent 70%,
    rgba(59, 130, 246, 0.4) 90%,
    rgba(59, 130, 246, 0.6) 100%
  ) !important;
  pointer-events: all !important;
  cursor: not-allowed !important;
  overflow: hidden !important;
}

.nr-webscraper-control-overlay__label {
  position: fixed;
  top: 16px;
  right: 16px;
  background: rgba(17, 24, 39, 0.98);
  backdrop-filter: blur(16px);
  color: #fff;
  padding: 12px 20px;
  border-radius: 12px;
  border: 2px solid rgba(255, 255, 255, 0.15);
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  font-size: 14px;
  font-weight: 600;
  letter-spacing: 0.3px;
  box-shadow: 0 25px 60px rgba(0, 0, 0, 0.4),
              0 0 0 1px rgba(255, 255, 255, 0.08),
              inset 0 1px 0 rgba(255, 255, 255, 0.1);
  z-index: 2147483647 !important;
  animation: nr-webscraper-info-slide-in 0.3s ease-out;
}

.nr-webscraper-control-overlay__icon {
  width: 18px;
  height: 18px;
  border: 2px solid currentColor;
  border-radius: 50%;
  border-top-color: transparent;
  animation: nr-webscraper-control-spin 1s linear infinite;
}

@keyframes nr-webscraper-control-spin {
  to { transform: rotate(360deg); }
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
    // 操作IDを増加させて、進行中の非同期操作をキャンセル
    ++this.highlightOperationId;

    // timerSetTimeoutで設定したタイマーはtimerClearTimeoutで解除する
    if (this.highlightCleanupTimer !== null) {
      timerClearTimeout(this.highlightCleanupTimer);
    }
    this.highlightCleanupTimer = null;

    // 先に参照を保存してからクリア（レース条件を防ぐ）
    const overlaysToRemove = [
      ...(this.highlightOverlay ? [this.highlightOverlay] : []),
      ...this.highlightOverlays,
    ];
    const callbacksToRun = [...this.highlightCleanupCallbacks];

    // 参照を即座にクリア
    this.highlightOverlay = null;
    this.highlightOverlays = [];
    this.highlightCleanupCallbacks = [];

    // 一時ハイライトをフェードアウト
    if (overlaysToRemove.length > 0) {
      overlaysToRemove.forEach((overlay) => {
        // DeadObject対策: try-catchでisConnectedチェックを囲む
        try {
          if (overlay?.isConnected) {
            overlay.style.setProperty(
              "transition",
              "opacity 300ms ease-out, transform 300ms ease-out",
            );
            overlay.style.setProperty("opacity", "0");
            overlay.style.setProperty("transform", "scale(0.98)");
            timerSetTimeout(() => {
              try {
                if (overlay?.isConnected) {
                  overlay.remove();
                }
              } catch {
                // DeadObject - 既に削除済み、無視
              }
            }, 300);
          }
        } catch {
          // DeadObject - 既にナビゲーション等で無効化されているため無視
        }
      });
    }

    // クリーンアップコールバックを実行
    for (const cleanup of callbacksToRun) {
      try {
        cleanup();
      } catch {
        // ignore cleanup errors
      }
    }
  }

  /**
   * Gracefully cleanup highlight with minimum display time guarantee.
   * If the highlight hasn't been visible for the minimum duration,
   * schedules cleanup for later. Otherwise, cleans up immediately.
   */
  private gracefulCleanupHighlight(): void {
    const elapsed = Date.now() - this.highlightStartTime;
    const remaining =
      NRWebScraperChild.MINIMUM_HIGHLIGHT_DURATION - elapsed;

    if (remaining > 0 && this.highlightStartTime > 0) {
      // ハイライトがまだ最小表示時間に達していない場合、遅延してクリーンアップ
      // 既存のタイマーがあればクリア
      if (this.highlightCleanupTimer !== null) {
        timerClearTimeout(this.highlightCleanupTimer);
      }

      this.highlightCleanupTimer = timerSetTimeout(() => {
        this.cleanupHighlight();
      }, remaining);
    } else {
      // 最小表示時間を超えている場合は即座にクリーンアップ
      this.cleanupHighlight();
    }
  }

  /**
   * Shows a full-screen control overlay that blocks user interaction
   * and indicates that the tab is being controlled by AI.
   */
  private showControlOverlay(): void {
    const doc = this.document;
    if (!doc) return;

    // Already showing
    if (this.controlOverlay?.isConnected) return;

    this.ensureHighlightStyle();

    // Create overlay
    const overlay = doc.createElement("div");
    overlay.className = "nr-webscraper-control-overlay";
    overlay.id = "nr-webscraper-control-overlay";
    overlay.tabIndex = -1; // Make focusable to capture key events

    // Create label
    const label = doc.createElement("div");
    label.className = "nr-webscraper-control-overlay__label";
    label.textContent = "Floorpが操作中...";

    (doc.body ?? doc.documentElement)?.appendChild(overlay);
    (doc.body ?? doc.documentElement)?.appendChild(label);

    (overlay as any).focus();

    // Prevent scrolling on the document
    const body = doc.body;
    const html = doc.documentElement as HTMLElement | null;
    if (body) body.style.setProperty("overflow", "hidden", "important");
    if (html) html.style.setProperty("overflow", "hidden", "important");

    // Prevent right-click context menu
    overlay.addEventListener("contextmenu", (e) => e.preventDefault(), {
      capture: true,
    });

    // Prevent scrolling via wheel or touch
    const preventScroll = (e: any) => {
      e.preventDefault();
      e.stopPropagation();
    };
    overlay.addEventListener("wheel", preventScroll, {
      passive: false,
      capture: true,
    });
    overlay.addEventListener("touchmove", preventScroll, {
      passive: false,
      capture: true,
    });

    // Prevent scrolling via keyboard
    const scrollKeys = [" ", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "PageUp", "PageDown", "Home", "End"];
    overlay.addEventListener("keydown", (e: any) => {
      if (scrollKeys.includes(e.key)) {
        e.preventDefault();
        e.stopPropagation();
      }
    }, { capture: true });

    this.controlOverlay = overlay;
  }

  /**
   * Hides the control overlay and restores user interaction.
   */
  private hideControlOverlay(): void {
    const doc = this.document;
    if (!doc) return;

    // Remove overlay
    if (this.controlOverlay?.isConnected) {
      // Restore scrolling
      const body = doc.body;
      const html = doc.documentElement as HTMLElement | null;
      if (body) body.style.removeProperty("overflow");
      if (html) html.style.removeProperty("overflow");

      this.controlOverlay.style.setProperty("transition", "opacity 300ms ease-out");
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

    // Remove label
    const label = doc.querySelector(".nr-webscraper-control-overlay__label");
    if (label) {
      (label as HTMLElement).style.setProperty("transition", "opacity 200ms ease-out");
      (label as HTMLElement).style.setProperty("opacity", "0");
      timerSetTimeout(() => {
        try {
          label?.remove();
        } catch {
          // ignore
        }
      }, 200);
    }
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
        timerSetTimeout(() => this.hideInfoPanel(), 4000),
      );
    } catch (error) {
      console.warn("NRWebScraperChild: Failed to show info panel", error);
    }
  }

  private hideInfoPanel(): void {
    // timerSetTimeoutで設定したタイマーはtimerClearTimeoutで解除する
    if (this.infoPanelCleanupTimer !== null) {
      timerClearTimeout(this.infoPanelCleanupTimer);
    }
    this.infoPanelCleanupTimer = null;

    // 先に参照を保存してクリア
    const panel = this.infoPanel;
    this.infoPanel = null;

    // DeadObject対策: try-catchでisConnectedチェックを囲む
    try {
      if (panel?.isConnected) {
        panel.remove();
      }
    } catch {
      // DeadObject - 既にナビゲーション等で無効化されているため無視
    }
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
    this.gracefulCleanupHighlight();

    // gracefulCleanupHighlight()後に操作IDが増加する可能性があるので、その後で現在の操作IDを記録
    const currentOperationId = this.highlightOperationId;

    // ハイライト開始時刻を記録
    this.highlightStartTime = Date.now();

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

    // showInfoPanel await後に操作IDチェック - 新しい操作が開始されていたらオーバーレイ作成をスキップ
    if (currentOperationId !== this.highlightOperationId) {
      this.hideInfoPanel(); // キャンセル時は表示した情報パネルを隠す
      return true;
    }

    for (const target of targets) {
      // Check if target is still alive (not a DeadObject from a previous page)
      try {
        void target.nodeType;
      } catch {
        // Skip dead objects
        continue;
      }

      const overlay = doc.createElement("div");
      overlay.className = "nr-webscraper-highlight-overlay";
      if (colorClass) {
        overlay.classList.add(colorClass);
      }

      const padding = options.padding;

      (doc.body ?? doc.documentElement)?.append(overlay);
      // オーバーレイをDOMに追加したら即座に配列にも追加（cleanupHighlightで確実に取得できるように）
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
    }

    // Scroll to first element (with DeadObject safety check)
    if (
      options.scrollBehavior !== "none" &&
      targets.length > 0 &&
      this.highlightOverlays.length > 0
    ) {
      try {
        // Find first alive target to scroll to
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
        this.cleanupHighlight();
        this.hideInfoPanel(); // オーバーレイと一緒に情報パネルも非表示
      }, options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    // 新しい操作が開始された場合はここで早期リターン
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

    // Check if target is still alive (not a DeadObject from a previous page)
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
    this.gracefulCleanupHighlight();

    // gracefulCleanupHighlight()後に操作IDが増加する可能性があるので、その後で現在の操作IDを記録
    // 新しい操作が開始されたら、古い操作は自動的にキャンセルされる
    const currentOperationId = this.highlightOperationId;

    // ハイライト開始時刻を記録
    this.highlightStartTime = Date.now();

    // Show info panel (await before creating overlay)
    if (showPanel) {
      await this.showInfoPanel(options.action, undefined, elementInfo, 1);

      // showInfoPanel await後に操作IDチェック
      if (currentOperationId !== this.highlightOperationId) {
        this.hideInfoPanel(); // キャンセル時は表示した情報パネルを隠す
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
      label.textContent = await this.getActionLabel(options.action);
      overlay.append(label);

      // ラベル取得後に操作IDチェック
      if (currentOperationId !== this.highlightOperationId) {
        this.hideInfoPanel(); // キャンセル時は表示した情報パネルを隠す
        return true;
      }
    }

    (doc.body ?? doc.documentElement)?.append(overlay);
    // オーバーレイをDOMに追加したら即座に追跡（cleanupHighlightで確実に取得できるように）
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
      timerSetTimeout(() => {
        this.cleanupHighlight();
        this.hideInfoPanel(); // オーバーレイと一緒に情報パネルも非表示
      }, options.duration),
    );

    await new Promise<void>((resolve) => {
      win.requestAnimationFrame(() => resolve());
    });

    // 新しい操作が開始された場合はここで早期リターン
    // オーバーレイは既にDOMに追加されているが、タイマーでクリーンアップされる
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
   * Handles DOM events derived from JSWindowActor registration.
   * Required to prevent "Property 'handleEvent' is not callable" errors.
   */
  handleEvent(_event: Event): void {
    // No-op: We only listen to trigger actor creation or specific side-effects
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

    // SPAナビゲーション対応: pagehideイベントでクリーンアップ
    const win = this.contentWindow;
    if (win) {
      this.pageHideHandler = () => {
        this.cleanupHighlight();
        this.hideInfoPanel();
      };
      win.addEventListener("pagehide", this.pageHideHandler);
    }
  }

  /**
   * Called when the actor is about to be destroyed
   *
   * This method ensures all resources are cleaned up properly
   * to prevent memory leaks when the content window is closed.
   */
  willDestroy() {
    // pagehideイベントリスナーを解除
    const win = this.contentWindow;
    if (win && this.pageHideHandler) {
      try {
        win.removeEventListener("pagehide", this.pageHideHandler);
      } catch {
        // DeadObject - 無視
      }
    }
    this.pageHideHandler = null;

    // Clean up all highlight overlays and timers
    this.cleanupHighlight();
    this.hideInfoPanel();

    // Remove style element if still connected
    try {
      if (this.highlightStyleElement?.isConnected) {
        this.highlightStyleElement.remove();
      }
    } catch {
      // DeadObject - 無視
    }
    this.highlightStyleElement = null;
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
          return this.inputElement(message.data.selector, message.data.value, {
            typingMode: message.data.typingMode,
            typingDelayMs: message.data.typingDelayMs,
          });
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
          return this.fillForm(message.data.formData, {
            typingMode: message.data.typingMode,
            typingDelayMs: message.data.typingDelayMs,
          });
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
        this.hideControlOverlay();
        return true;
      case "WebScraper:ShowControlOverlay":
        this.showControlOverlay();
        return true;
      case "WebScraper:HideControlOverlay":
        this.hideControlOverlay();
        return true;
      case "WebScraper:GetAttribute":
        if (message.data?.selector && message.data?.attributeName) {
          return this.getAttribute(
            message.data.selector,
            message.data.attributeName,
          );
        }
        break;
      case "WebScraper:IsVisible":
        if (message.data?.selector) {
          return this.isVisible(message.data.selector);
        }
        break;
      case "WebScraper:IsEnabled":
        if (message.data?.selector) {
          return this.isEnabled(message.data.selector);
        }
        break;
      case "WebScraper:ClearInput":
        if (message.data?.selector) {
          return this.clearInput(message.data.selector);
        }
        break;
      case "WebScraper:SelectOption":
        if (message.data?.selector && message.data?.optionValue !== undefined) {
          return this.selectOption(
            message.data.selector,
            message.data.optionValue,
          );
        }
        break;
      case "WebScraper:SetChecked":
        if (
          message.data?.selector &&
          typeof message.data?.checked === "boolean"
        ) {
          return this.setChecked(message.data.selector, message.data.checked);
        }
        break;
      case "WebScraper:HoverElement":
        if (message.data?.selector) {
          return this.hoverElement(message.data.selector);
        }
        break;
      case "WebScraper:ScrollToElement":
        if (message.data?.selector) {
          return this.scrollToElement(message.data.selector);
        }
        break;
      case "WebScraper:DoubleClick":
        if (message.data?.selector) {
          return this.doubleClickElement(message.data.selector);
        }
        break;
      case "WebScraper:RightClick":
        if (message.data?.selector) {
          return this.rightClickElement(message.data.selector);
        }
        break;
      case "WebScraper:Focus":
        if (message.data?.selector) {
          return this.focusElement(message.data.selector);
        }
        break;
      case "WebScraper:GetPageTitle":
        return this.getPageTitle();
      case "WebScraper:DragAndDrop":
        if (message.data?.selector && message.data?.targetSelector) {
          return this.dragAndDrop(
            message.data.selector,
            message.data.targetSelector,
          );
        }
        break;
      case "WebScraper:SetInnerHTML":
        if (
          message.data?.selector &&
          typeof message.data?.innerHTML === "string"
        ) {
          return this.setInnerHTML(
            message.data.selector,
            message.data.innerHTML,
          );
        }
        break;
      case "WebScraper:SetTextContent":
        if (
          message.data?.selector &&
          typeof message.data?.textContent === "string"
        ) {
          return this.setTextContent(
            message.data.selector,
            message.data.textContent,
          );
        }
        break;
      case "WebScraper:DispatchEvent":
        if (message.data?.selector && message.data?.eventType) {
          return this.dispatchEvent(
            message.data.selector,
            message.data.eventType,
            message.data.eventOptions,
          );
        }
        break;
      case "WebScraper:PressKey":
        if (message.data?.key) {
          return this.pressKey(message.data.key);
        }
        break;
      case "WebScraper:UploadFile":
        if (message.data?.selector && message.data?.filePath) {
          return this.uploadFile(message.data.selector, message.data.filePath);
        }
        break;
      case "WebScraper:SetCookieString":
        if (message.data?.cookieString) {
          return this.setCookieString(
            message.data.cookieString,
            message.data.cookieName,
            message.data.cookieValue,
          );
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
  async inputElement(
    selector: string,
    value: string,
    options: {
      typingMode?: boolean;
      typingDelayMs?: number;
      skipHighlight?: boolean;
    } = {},
  ): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | HTMLSelectElement
        | null;
      if (!element) {
        return false;
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      if (!options.skipHighlight) {
        const truncatedValue = this.truncate(value, 50);
        const elementInfo = await this.translate("inputValueSet", {
          value: truncatedValue,
        });
        const highlightOptions = this.getHighlightOptions("Input");

        await this.applyHighlight(element, highlightOptions, elementInfo);
      }

      const win = this.contentWindow;
      if (!win) return false;

      // Re-verify element is still alive after potential navigation during highlight
      try {
        void element.nodeType;
      } catch {
        return false;
      }

      // Use wrappedJSObject for page context access
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;

      // Select elements are handled via selectOption for better semantics
      if (element instanceof win.HTMLSelectElement) {
        return this.selectOption(selector, value);
      }


      const HTMLInputProto = rawWin.HTMLInputElement?.prototype;
      const HTMLTextAreaProto = rawWin.HTMLTextAreaElement?.prototype;

      const nativeInputValueSetter = HTMLInputProto
        ? Object.getOwnPropertyDescriptor(HTMLInputProto, "value")?.set
        : undefined;
      const nativeTextAreaValueSetter = HTMLTextAreaProto
        ? Object.getOwnPropertyDescriptor(HTMLTextAreaProto, "value")?.set
        : undefined;

      const setter =
        rawElement instanceof rawWin.HTMLInputElement
          ? nativeInputValueSetter
          : nativeTextAreaValueSetter;

      const typingMode = options.typingMode === true;
      const typingDelay =
        typeof options.typingDelayMs === "number"
          ? Math.max(0, options.typingDelayMs)
          : 25;

      const InputEv = (rawWin.InputEvent ?? null) as typeof InputEvent | null;
      const EventCtor = (rawWin.Event ?? globalThis.Event) as typeof Event;
      const KeyboardEv = (rawWin.KeyboardEvent ??
        globalThis.KeyboardEvent) as typeof KeyboardEvent;
      const FocusEv = (rawWin.FocusEvent ??
        globalThis.FocusEvent) as typeof FocusEvent;

      const dispatchBeforeInput = (data: string) => {
        try {
          if (InputEv) {
            rawElement.dispatchEvent(
              new InputEv("beforeinput", {
                bubbles: true,
                cancelable: true,
                inputType: "insertText",
                data,
              }),
            );
          }
        } catch {
          // ignore
        }
      };

      const setValue = (v: string) => {
        if (setter) {
          setter.call(rawElement, v);
        } else {
          rawElement.value = v;
        }
      };

      if (typingMode) {
        setValue("");
        for (const ch of value.split("")) {
          dispatchBeforeInput(ch);
          setValue(rawElement.value + ch);
          rawElement.dispatchEvent(
            new EventCtor("input", { bubbles: true }),
          );
          rawElement.dispatchEvent(
            new KeyboardEv("keydown", { key: ch, bubbles: true }),
          );
          rawElement.dispatchEvent(
            new KeyboardEv("keyup", { key: ch, bubbles: true }),
          );
          if (typingDelay > 0) {
            await new Promise((r) => timerSetTimeout(r, typingDelay));
          }
        }
        rawElement.dispatchEvent(
          new EventCtor("change", { bubbles: true }),
        );
      } else {
        dispatchBeforeInput(value);
        setValue(value);
        rawElement.dispatchEvent(
          new EventCtor("input", { bubbles: true }),
        );
        rawElement.dispatchEvent(
          new EventCtor("change", { bubbles: true }),
        );
      }

      rawElement.dispatchEvent(
        new FocusEv("blur", { bubbles: true }),
      );
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

      // Re-verify element is still alive after translation delay
      try {
        void element.nodeType;
      } catch {
        return false;
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

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

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      let clickDispatched = this.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );

      // Force native click via wrappedJSObject to ensure compatibility with sites that rely on simple click events (like DevTools usage)
      try {
         const rawElement = (element as any).wrappedJSObject ?? element;
         rawElement.click();
         clickDispatched = true;
      } catch (e) {
         // ignore
      }

      if (!clickDispatched) {
        try {
          const rawElement = (element as any).wrappedJSObject ?? element;
          rawElement.click();
          clickDispatched = true;
        } catch (e) {
          void e;
        }
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

      // エフェクトが最低限表示されるまで短時間待機（完全な完了は待たない）
      await Promise.race([
        effectPromise,
        new Promise((resolve) => timerSetTimeout(resolve, 300)),
      ]);

      return success;
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
      const doc = this.document;
      if (!doc) return false;

      try {
        const element = doc.querySelector(selector);
        if (element) {
          return true;
        }
        // Wait 100ms before next check
        await new Promise((resolve) => timerSetTimeout(resolve, 100));
      } catch (e) {
        // If it's a DeadObject error, the page navigated
        if (
          e &&
          typeof e === "object" &&
          "message" in e &&
          typeof (e as { message?: unknown }).message === "string" &&
          ((e as { message: string }).message?.includes("dead object") ?? false)
        ) {
          return false;
        }
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
        if (!win) return false;
        const doc = win.document;
        if (
          doc &&
          doc.documentElement &&
          (doc.body ||
            doc.readyState === "interactive" ||
            doc.readyState === "complete")
        ) {
          return true;
        }
        await new Promise((r) => timerSetTimeout(r, 100));
      } catch (e) {
        if (
          e &&
          typeof e === "object" &&
          "message" in e &&
          typeof (e as { message?: unknown }).message === "string" &&
          ((e as { message: string }).message?.includes("dead object") ?? false)
        ) {
          return false;
        }
        await new Promise((r) => timerSetTimeout(r, 100));
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

      // getBoundingClientRect returns viewport-relative coordinates
      // drawWindow needs absolute page coordinates, so add scroll offset
      const scrollX = this.contentWindow.scrollX || 0;
      const scrollY = this.contentWindow.scrollY || 0;

      ctx.drawWindow(
        this.contentWindow,
        rect.left + scrollX,
        rect.top + scrollY,
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
    options: { typingMode?: boolean; typingDelayMs?: number } = {},
  ): Promise<boolean> {
    try {
      let allFilled = true;
      const selectors = Object.keys(formData);
      const fieldCount = selectors.length;
      const win = this.contentWindow;
      const doc = this.document;
      const action = "Fill";
      const highlightOptions = this.getHighlightOptions(action);

      // Ensure document is minimally ready before filling
      await this.waitForReady(5000);

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
        let element = this.document?.querySelector(selector) as
          | HTMLInputElement
          | HTMLTextAreaElement
          | HTMLSelectElement
          | null;

        // If not found immediately, wait briefly for it to appear
        if (!element) {
          const waited = await this.waitForElement(selector, 3000);
          if (waited) {
            element = this.document?.querySelector(selector) as
              | HTMLInputElement
              | HTMLTextAreaElement
              | HTMLSelectElement
              | null;
          }
        }

        if (element) {
          const elementInfo = await this.translate("formFieldProgress", {
            current: i + 1,
            total: fieldCount,
            value: this.truncate(value, 30),
          });
          if (fieldCount > 1 && doc) {
            await this.showInfoPanel(
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

          let success = false;
          if (
            element instanceof (win?.HTMLSelectElement ?? HTMLSelectElement)
          ) {
            success = await this.selectOption(selector, value, {
              skipHighlight: true,
            });
          } else {
            success = await this.inputElement(selector, value, {
              ...options,
              skipHighlight: true,
            });
          }
          if (!success) {
            allFilled = false;
          }

          // 各フィールド入力後に1.2秒の遅延（ユーザーがエフェクトを確認できるように）
          if (i < selectors.length - 1) {
            await new Promise<void>((resolve) => {
              timerSetTimeout(() => resolve(), 1200);
            });
          }
        } else {
          console.warn(
            `NRWebScraperChild: Element not found for selector: ${selector}`,
          );
          allFilled = false;
        }
      }

      // 最終的な値を確認し、失敗したフィールドがあれば簡易的に再設定する
      if (!win) {
        return allFilled;
      }

      let finalOk = true;
      for (const selector of selectors) {
        const expectedValue = formData[selector];

        const element = this.document?.querySelector(selector) as
          | HTMLInputElement
          | HTMLTextAreaElement
          | HTMLSelectElement
          | null;

        if (!element) {
          finalOk = false;
          continue;
        }

        const rawWin = (win as any)?.wrappedJSObject ?? win;
        const rawElement = (element as any)?.wrappedJSObject ?? element;
        const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;
        const FocusEv = (rawWin?.FocusEvent ?? globalThis.FocusEvent) as typeof FocusEvent;

        const currentValue =
          win && element instanceof win.HTMLSelectElement
            ? element.value
            : (element.value ?? "");

        if (currentValue === expectedValue) {
          continue;
        }

        try {
          if (win && element instanceof win.HTMLSelectElement) {
            const selectEl = element as HTMLSelectElement;
            const options = Array.from(selectEl.options) as HTMLOptionElement[];
            const targetOpt =
              options.find((opt) => opt.value === expectedValue) ?? null;
            if (targetOpt) {
              selectEl.value = targetOpt.value;
            } else {
              selectEl.value = expectedValue;
            }
          } else {
            const HTMLInputProto = rawWin.HTMLInputElement?.prototype;
            const HTMLTextAreaProto = rawWin.HTMLTextAreaElement?.prototype;

            const nativeInputValueSetter = HTMLInputProto
              ? Object.getOwnPropertyDescriptor(HTMLInputProto, "value")?.set
              : undefined;
            const nativeTextAreaValueSetter = HTMLTextAreaProto
              ? Object.getOwnPropertyDescriptor(HTMLTextAreaProto, "value")?.set
              : undefined;

            const setter =
              rawElement instanceof rawWin.HTMLInputElement
                ? nativeInputValueSetter
                : nativeTextAreaValueSetter;

            if (setter) {
              setter.call(rawElement, expectedValue);
            } else {
              rawElement.value = expectedValue;
            }
          }

          rawElement.dispatchEvent(
            new EventCtor("input", { bubbles: true }),
          );
          rawElement.dispatchEvent(
            new EventCtor("change", { bubbles: true }),
          );
          rawElement.dispatchEvent(
            new FocusEv("blur", { bubbles: true }),
          );
        } catch (_err) {
          try {
            if (win && element instanceof win.HTMLSelectElement) {
              const selectEl = element as HTMLSelectElement;
              const options = Array.from(
                selectEl.options,
              ) as HTMLOptionElement[];
              const targetOpt =
                options.find((opt) => opt.value === expectedValue) ?? null;
              if (targetOpt) {
                selectEl.value = targetOpt.value;
              } else {
                selectEl.value = expectedValue;
              }
            } else {
              const nativeInputValueSetter = win?.HTMLInputElement.prototype
                ? Object.getOwnPropertyDescriptor(
                    win.HTMLInputElement.prototype,
                    "value",
                  )?.set
                : undefined;
              const nativeTextAreaValueSetter = win?.HTMLTextAreaElement
                .prototype
                ? Object.getOwnPropertyDescriptor(
                    win.HTMLTextAreaElement.prototype,
                    "value",
                  )?.set
                : undefined;
              if (
                nativeInputValueSetter &&
                win &&
                element instanceof win.HTMLInputElement
              ) {
                nativeInputValueSetter.call(element, expectedValue);
              } else if (
                nativeTextAreaValueSetter &&
                win &&
                element instanceof win.HTMLTextAreaElement
              ) {
                nativeTextAreaValueSetter.call(element, expectedValue);
              } else {
                element.value = expectedValue;
              }
            }
          } catch (e) {
            console.error(
              `NRWebScraperChild: Error setting value for selector ${selector}`,
              e,
            );
            finalOk = false;
          }
        }
      }

      return allFilled && finalOk;
    } catch (e) {
      console.error("NRWebScraperChild: Error filling form:", e);
      return false;
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

  /**
   * Gets the value of an input/textarea/select element.
   */
  async getValue(selector: string): Promise<string | null> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | HTMLSelectElement
        | null;
      if (!element) return null;

      // Show highlight effect and wait for it to be visible
      const elementInfo = await this.translate("inspectGetValue", { selector });
      await this.applyHighlight(
        element,
        { action: "InspectPeek" },
        elementInfo,
        true,
      );

      const win = this.contentWindow;
      if (win && element instanceof win.HTMLSelectElement) {
        return element.value ?? "";
      }

      if (
        win &&
        (element instanceof win.HTMLInputElement ||
          element instanceof win.HTMLTextAreaElement)
      ) {
        return element.value ?? "";
      }

      // Fallback: if the element exposes a value property, return it; otherwise textContent
      // This helps in cases where the realm differs or element type is unusual.
      const maybeVal = (element as { value?: unknown }).value;
      if (maybeVal !== undefined) {
        return typeof maybeVal === "string" ? maybeVal : String(maybeVal ?? "");
      }

      return element.textContent ?? "";
    } catch (e) {
      console.error("NRWebScraperChild: Error getting value:", e);
      return null;
    }
  }

  /**
   * Gets the value of a specific attribute from an element
   *
   * @param selector - CSS selector to find the target element
   * @param attributeName - Name of the attribute to retrieve
   * @returns string | null - The attribute value, or null if not found
   */
  getAttribute(selector: string, attributeName: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.runAsyncInspection(element, "inspectGetElement", { selector });
        return element.getAttribute(attributeName);
      }
      return null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting attribute:", e);
      return null;
    }
  }

  /**
   * Checks if an element is visible in the viewport
   *
   * An element is considered visible if:
   * - It exists in the DOM
   * - It has non-zero dimensions
   * - It is not hidden via CSS (display: none, visibility: hidden, opacity: 0)
   *
   * @param selector - CSS selector to find the target element
   * @returns boolean - True if the element is visible, false otherwise
   */
  isVisible(selector: string): boolean {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      // Check if element has dimensions
      const rect = element.getBoundingClientRect();
      if (rect.width === 0 || rect.height === 0) return false;

      // Check computed styles
      const style = this.contentWindow?.getComputedStyle(element);
      if (!style) return false;

      if (style.getPropertyValue("display") === "none") return false;
      if (style.getPropertyValue("visibility") === "hidden") return false;
      if (style.getPropertyValue("opacity") === "0") return false;

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error checking visibility:", e);
      return false;
    }
  }

  /**
   * Checks if an element is enabled (not disabled)
   *
   * This is useful for form elements like buttons, inputs, and selects.
   *
   * @param selector - CSS selector to find the target element
   * @returns boolean - True if the element is enabled, false if disabled or not found
   */
  isEnabled(selector: string): boolean {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      // Check the disabled property for form elements
      if ("disabled" in element) {
        return !(element as HTMLInputElement | HTMLButtonElement).disabled;
      }

      // Check aria-disabled attribute for other elements
      const ariaDisabled = element.getAttribute("aria-disabled");
      if (ariaDisabled === "true") return false;

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error checking enabled state:", e);
      return false;
    }
  }

  /**
   * Clears the value of an input or textarea element
   *
   * This method clears the input and triggers appropriate events
   * to ensure frameworks detect the change.
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if cleared successfully, false otherwise
   */
  async clearInput(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (!element) return false;

      // Check if it's a valid input element
      if (!("value" in element)) return false;

      const elementInfo = await this.translate("inputValueSet", {
        value: "(cleared)",
      });
      const options = this.getHighlightOptions("Input");

      await this.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      if (!win) return false;

      // Reactなどのフレームワークのために、ネイティブのsetterを使用して値をセットする
      const nativeInputValueSetter = win.HTMLInputElement.prototype
        ? Object.getOwnPropertyDescriptor(
            win.HTMLInputElement.prototype,
            "value",
          )?.set
        : undefined;
      const nativeTextAreaValueSetter = win.HTMLTextAreaElement.prototype
        ? Object.getOwnPropertyDescriptor(
            win.HTMLTextAreaElement.prototype,
            "value",
          )?.set
        : undefined;

      const setter =
        element instanceof win.HTMLInputElement
          ? nativeInputValueSetter
          : nativeTextAreaValueSetter;

      if (setter) {
        setter.call(element, "");
      } else {
        element.value = "";
      }

      // Trigger events to notify frameworks
      element.dispatchEvent(new Event("input", { bubbles: true }));
      element.dispatchEvent(new Event("change", { bubbles: true }));
      element.dispatchEvent(new FocusEvent("blur", { bubbles: true }));

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error clearing input:", e);
      return false;
    }
  }

  /**
   * Selects an option in a select element
   *
   * This method finds a select element and sets its value to the specified option.
   * It triggers appropriate events to ensure frameworks detect the change.
   *
   * @param selector - CSS selector to find the target select element
   * @param value - The value of the option to select
   * @returns Promise<boolean> - True if selected successfully, false otherwise
   */
  async selectOption(
    selector: string,
    value: string,
    opts: { skipHighlight?: boolean } = {},
  ): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLSelectElement | null;
      if (!element) return false;

      // Check if it's a valid select element
      if (element.tagName !== "SELECT") return false;

      // Resolve target option: value -> text -> label -> partial text
      const options = Array.from(element.options) as HTMLOptionElement[];
      let targetOpt = options.find((opt) => opt.value === value);
      if (!targetOpt) {
        targetOpt = options.find(
          (opt) => (opt.textContent ?? "").trim() === value,
        );
      }
      if (!targetOpt) {
        targetOpt = options.find((opt) => (opt.label ?? "").trim() === value);
      }
      if (!targetOpt) {
        const lower = value.toLowerCase();
        targetOpt = options.find((opt) =>
          (opt.textContent ?? "").toLowerCase().includes(lower),
        );
      }
      if (!targetOpt) return false;

      if (!opts.skipHighlight) {
        const elementInfo = await this.translate("selectOption", {
          value: targetOpt.value,
        });
        const optionsHighlight = this.getHighlightOptions("Input");

        await this.applyHighlight(element, optionsHighlight, elementInfo);
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const win = this.contentWindow;
      if (!win) return false;

      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      const HTMLSelectProto = rawWin.HTMLSelectElement?.prototype;
      const nativeSelectValueSetter = HTMLSelectProto
        ? Object.getOwnPropertyDescriptor(HTMLSelectProto, "value")?.set
        : undefined;

      if (nativeSelectValueSetter) {
        nativeSelectValueSetter.call(rawElement, targetOpt.value);
      } else {
        rawElement.value = targetOpt.value;
      }

      // Trigger events to notify frameworks
      rawElement.dispatchEvent(
        new EventCtor("input", { bubbles: true }),
      );
      rawElement.dispatchEvent(
        new EventCtor("change", { bubbles: true }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error selecting option:", e);
      return false;
    }
  }

  /**
   * Sets the checked state of a checkbox or radio button
   *
   * This method finds a checkbox or radio input element and sets its checked state.
   * It triggers appropriate events to ensure frameworks detect the change.
   *
   * @param selector - CSS selector to find the target input element
   * @param checked - The desired checked state
   * @returns Promise<boolean> - True if set successfully, false otherwise
   */
  async setChecked(selector: string, checked: boolean): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element) return false;

      // Check if it's a valid checkbox or radio element
      if (element.tagName !== "INPUT") return false;
      if (element.type !== "checkbox" && element.type !== "radio") return false;

      const elementInfo = await this.translate("setChecked", {
        state: checked ? "checked" : "unchecked",
      });
      const options = this.getHighlightOptions("Click");

      await this.applyHighlight(element, options, elementInfo);

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const win = this.contentWindow;
      if (!win) return false;

      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;
      const MouseEv = (rawWin?.MouseEvent ??
        globalThis.MouseEvent) as typeof MouseEvent;

      const HTMLInputProto = rawWin.HTMLInputElement?.prototype;
      const nativeCheckedSetter = HTMLInputProto
        ? Object.getOwnPropertyDescriptor(HTMLInputProto, "checked")?.set
        : undefined;

      if (nativeCheckedSetter) {
        nativeCheckedSetter.call(rawElement, checked);
      } else {
        rawElement.checked = checked;
      }

      // Trigger events to notify frameworks
      rawElement.dispatchEvent(
        new EventCtor("input", { bubbles: true }),
      );
      rawElement.dispatchEvent(
        new EventCtor("change", { bubbles: true }),
      );

      // For radio buttons, we may need to trigger click event
      if (element.type === "radio" && checked) {
        rawElement.dispatchEvent(
          new MouseEv("click", { bubbles: true }),
        );
      }

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error setting checked state:", e);
      return false;
    }
  }

  /**
   * Press a key or key combination (e.g., "Control+Shift+R").
   */
  async pressKey(keyCombo: string): Promise<boolean> {
    try {
      const win = this.contentWindow;
      const doc = this.document;
      if (!win || !doc) return false;

      const parts = keyCombo
        .split("+")
        .map((p) => p.trim())
        .filter(Boolean);
      if (parts.length === 0) return false;
      const key = parts.pop() as string;
      const modifiers = parts;

      const active = (doc.activeElement as HTMLElement | null) ?? doc.body;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const activeRaw = (active as any)?.wrappedJSObject ?? active;
      const KeyboardEv = (rawWin?.KeyboardEvent ??
        globalThis.KeyboardEvent) as typeof KeyboardEvent;

      const dispatch = (type: string, opts: KeyboardEventInit) => {
        try {
          return (
            activeRaw?.dispatchEvent(
              new KeyboardEv(type, { ...opts, view: rawWin }),
            ) ?? false
          );
        } catch {
          return false;
        }
      };

      for (const mod of modifiers) {
        dispatch("keydown", { key: mod, code: mod, bubbles: true });
      }

      dispatch("keydown", { key, code: key, bubbles: true });
      dispatch("keypress", { key, code: key, bubbles: true });
      dispatch("keyup", { key, code: key, bubbles: true });

      for (const mod of modifiers.reverse()) {
        dispatch("keyup", { key: mod, code: mod, bubbles: true });
      }

      await Promise.resolve();
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error pressing key:", e);
      return false;
    }
  }

  /**
   * Uploads a file through input[type=file].
   */
  async uploadFile(selector: string, filePath: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element || element.tagName !== "INPUT" || element.type !== "file") {
        return false;
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const elementInfo = await this.translate("uploadFile", {
        value: this.truncate(filePath.split(/[\\/]/).pop() ?? filePath, 30),
      });
      const options = this.getHighlightOptions("Input");
      await this.applyHighlight(element, options, elementInfo);

      type MozFileInput = HTMLInputElement & {
        mozSetFileArray?: (files: File[]) => void;
      };
      const fileInput = element as MozFileInput;

      // Create File object from path and set via mozSetFileArray
      // (Following Mozilla's Marionette pattern - mozSetFileNameArray is not available in content scripts)
      try {
        const file = await File.createFromFileName(filePath);
        if (typeof fileInput.mozSetFileArray === "function") {
          fileInput.mozSetFileArray([file]);
        } else {
          console.error("NRWebScraperChild: mozSetFileArray not available");
          return false;
        }
      } catch (e) {
        console.error("NRWebScraperChild: Failed to create file from path:", e);
        return false;
      }

      element.dispatchEvent(new Event("input", { bubbles: true }));
      element.dispatchEvent(new Event("change", { bubbles: true }));
      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error uploading file:", e);
      return false;
    }
  }

  /**
   * Sets a cookie using document.cookie
   *
   * This method sets a cookie in the content process using the document.cookie API.
   * It's used as a fallback when Services.cookies.add() fails to store cookies.
   *
   * @param cookieString - The full cookie string (e.g., "name=value; Path=/; Domain=example.com")
   * @param cookieName - The cookie name for verification
   * @param cookieValue - The cookie value for verification
   * @returns boolean - True if the cookie was set successfully, false otherwise
   */
  setCookieString(
    cookieString: string,
    cookieName?: string,
    cookieValue?: string,
  ): boolean {
    try {
      if (!this.contentWindow?.document) {
        return false;
      }

      // Set the cookie via document.cookie
      this.contentWindow.document.cookie = cookieString;

      // Verify if the cookie was set (if name and value were provided)
      if (cookieName && cookieValue) {
        const cookies = this.contentWindow.document.cookie;
        return cookies.includes(`${cookieName}=${cookieValue}`);
      }

      return true;
    } catch {
      return false;
    }
  }

  /**
   * Simulates a mouse hover over an element
   *
   * This method finds an element and dispatches mouse events to simulate
   * a hover interaction. This can trigger CSS :hover states and JavaScript
   * event handlers.
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if hover was simulated successfully, false otherwise
   */
  async hoverElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translate("hoverElement", {});
      const options = this.getHighlightOptions("Inspect");

      await this.applyHighlight(element, options, elementInfo);

      // Get element position for mouse event coordinates
      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const MouseEv = (rawWin?.MouseEvent ??
        globalThis.MouseEvent) as typeof MouseEvent;

      // Dispatch mouseenter and mouseover events use raw element
      rawElement.dispatchEvent(
        new MouseEv("mouseenter", {
          bubbles: false,
          cancelable: false,
          clientX: centerX,
          clientY: centerY,
          view: rawWin as Window & typeof globalThis,
        }),
      );
      rawElement.dispatchEvent(
        new MouseEv("mouseover", {
          bubbles: true,
          cancelable: true,
          clientX: centerX,
          clientY: centerY,
          view: rawWin as Window & typeof globalThis,
        }),
      );
      rawElement.dispatchEvent(
        new MouseEv("mousemove", {
          bubbles: true,
          cancelable: true,
          clientX: centerX,
          clientY: centerY,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error hovering element:", e);
      return false;
    }
  }

  /**
   * Scrolls the page to make an element visible
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if scrolled successfully
   */
  async scrollToElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translate("scrollToElement", {});
      const options = this.getHighlightOptions("Inspect");

      // Scroll element into view first
      element.scrollIntoView({ behavior: "smooth", block: "center" });

      // Apply highlight after scrolling
      await this.applyHighlight(element, options, elementInfo);

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error scrolling to element:", e);
      return false;
    }
  }

  /**
   * Performs a double click on an element
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if double clicked successfully
   */
  async doubleClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translate("doubleClickElement", {});
      const options = this.getHighlightOptions("Click");

      await this.applyHighlight(element, options, elementInfo);

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      // Two click sequences followed by dblclick for better compatibility
      this.dispatchPointerClickSequence(element, centerX, centerY, 0);
      this.dispatchPointerClickSequence(element, centerX, centerY, 0);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const MouseEv = (rawWin?.MouseEvent ??
        globalThis.MouseEvent) as typeof MouseEvent;

      rawElement.dispatchEvent(
        new MouseEv("dblclick", {
          bubbles: true,
          cancelable: true,
          clientX: centerX,
          clientY: centerY,
          view: rawWin as Window & typeof globalThis,
          detail: 2,
        }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error double clicking element:", e);
      return false;
    }
  }

  /**
   * Performs a right click (context menu) on an element
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if right clicked successfully
   */
  async rightClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translate("rightClickElement", {});
      const options = this.getHighlightOptions("Click");

      await this.applyHighlight(element, options, elementInfo);

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      // Pointer + mouse sequence with button=2, then contextmenu
      this.dispatchPointerClickSequence(element, centerX, centerY, 2);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const MouseEv = (rawWin?.MouseEvent ??
        globalThis.MouseEvent) as typeof MouseEvent;

      rawElement.dispatchEvent(
        new MouseEv("contextmenu", {
          bubbles: true,
          cancelable: true,
          clientX: centerX,
          clientY: centerY,
          view: rawWin as Window & typeof globalThis,
          button: 2,
        }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error right clicking element:", e);
      return false;
    }
  }

  /**
   * Focuses on an element
   *
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if focused successfully
   */
  async focusElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as HTMLElement;
      if (!element) return false;

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const FocusEv = (rawWin?.FocusEvent ??
        globalThis.FocusEvent) as typeof FocusEvent;

      this.scrollIntoViewIfNeeded(element);

      const elementInfo = await this.translate("focusElement", {});
      const options = this.getHighlightOptions("Input");

      await this.applyHighlight(element, options, elementInfo);

      // Force focus via wrappedJSObject
      if (typeof rawElement.focus === "function") {
        rawElement.focus();
      } else {
        element.focus();
      }

      rawElement.dispatchEvent(
        new FocusEv("focus", { bubbles: false }),
      );
      rawElement.dispatchEvent(
        new FocusEv("focusin", { bubbles: true }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error focusing element:", e);
      return false;
    }
  }

  /**
   * Gets the page title
   *
   * @returns string | null - The page title or null
   */
  getPageTitle(): string | null {
    try {
      return this.document?.title ?? null;
    } catch (e) {
      console.error("NRWebScraperChild: Error getting page title:", e);
      return null;
    }
  }

  /**
   * Performs a drag and drop operation between two elements
   *
   * @param sourceSelector - CSS selector for the source element
   * @param targetSelector - CSS selector for the target element
   * @returns Promise<boolean> - True if drag and drop was successful
   */
  async dragAndDrop(
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean> {
    try {
      const source = this.document?.querySelector(sourceSelector) as HTMLElement;
      const target = this.document?.querySelector(targetSelector) as HTMLElement;

      if (!source || !target) return false;

      this.scrollIntoViewIfNeeded(source);
      this.scrollIntoViewIfNeeded(target);

      const elementInfo = await this.translate("dragAndDrop", {});
      const options = this.getHighlightOptions("Input");

      await this.applyHighlight(source, options, elementInfo);
      await this.applyHighlight(target, options, elementInfo);

      const sourceRect = source.getBoundingClientRect();
      const targetRect = target.getBoundingClientRect();
      const sourceX = sourceRect.left + sourceRect.width / 2;
      const sourceY = sourceRect.top + sourceRect.height / 2;
      const targetX = targetRect.left + targetRect.width / 2;
      const targetY = targetRect.top + targetRect.height / 2;

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawSource = (source as any)?.wrappedJSObject ?? source;
      const rawTarget = (target as any)?.wrappedJSObject ?? target;
      
      const DragEv = (rawWin?.DragEvent ??
        globalThis.DragEvent) as typeof DragEvent;
      const DataTransferCtor = (rawWin?.DataTransfer ??
        globalThis.DataTransfer) as typeof DataTransfer;

      const dataTransfer = new DataTransferCtor();

      rawSource.dispatchEvent(
        new DragEv("dragstart", {
          bubbles: true,
          cancelable: true,
          clientX: sourceX,
          clientY: sourceY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      rawSource.dispatchEvent(
        new DragEv("drag", {
          bubbles: true,
          cancelable: true,
          clientX: sourceX,
          clientY: sourceY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      rawTarget.dispatchEvent(
        new DragEv("dragenter", {
          bubbles: true,
          cancelable: true,
          clientX: targetX,
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      rawTarget.dispatchEvent(
        new DragEv("dragover", {
          bubbles: true,
          cancelable: true,
          clientX: targetX,
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      rawTarget.dispatchEvent(
        new DragEv("drop", {
          bubbles: true,
          cancelable: true,
          clientX: targetX,
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      rawSource.dispatchEvent(
        new DragEv("dragend", {
          bubbles: true,
          cancelable: false,
          clientX: targetX, // dragend usually happens at drop location
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error in drag and drop:", e);
      return false;
    }
  }

  /**
   * Sets the innerHTML of an element matching the given selector.
   * Useful for contenteditable elements like rich text editors.
   *
   * @param selector - CSS selector for the target element
   * @param html - HTML content to set
   * @returns boolean - true if successful, false otherwise
   */
  async setInnerHTML(selector: string, html: string): Promise<boolean> {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector) as HTMLElement | null;
      if (!element) {
        console.warn(
          `NRWebScraperChild: Element not found for setInnerHTML: ${selector}`,
        );
        return false;
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const elementInfo = await this.translate("inputValueSet", {
        value: this.truncate(html, 30),
      });
      const options = this.getHighlightOptions("Input");

      await this.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawDoc = (this.document as any)?.wrappedJSObject ?? this.document;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      // Attempt to use execCommand for better integration with rich text editors (like Slack/ProseMirror)
      try {
        if (rawWin && rawDoc.hasFocus()) {
          const selection = rawWin.getSelection();
          if (selection) {
            const range = rawDoc.createRange();
            range.selectNodeContents(rawElement);
            selection.removeAllRanges();
            selection.addRange(range);

            if (rawDoc.execCommand("insertHTML", false, html)) {
              // Ensure events are still fired for safety using page context constructors
              rawElement.dispatchEvent(
                new EventCtor("input", { bubbles: true, cancelable: true }),
              );
              rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));
              return true;
            }
          }
        }
      } catch (e) {
        console.warn("NRWebScraperChild: execCommand failed, falling back:", e);
      }

      const InputEv = (rawWin?.InputEvent ?? null) as typeof InputEvent | null;

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv("beforeinput", {
            bubbles: true,
            cancelable: true,
            inputType: "insertHTML",
            data: null,
          }),
        );
      }

      // Fallback: direct innerHTML setting
      if (rawElement !== element) {
         // If we have access to wrappedJSObject, set innerHTML on it directly?
         // Note: Setting innerHTML on wrappedJSObject might bypass some Xray protections but is what we want for direct page modification.
         rawElement.innerHTML = html;
      } else {
         element.innerHTML = html;
      }

      // Dispatch input event to trigger any listeners
      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv("input", {
            bubbles: true,
            cancelable: false,
            inputType: "insertHTML",
          }),
        );
      } else {
        rawElement.dispatchEvent(
          new EventCtor("input", { bubbles: true, cancelable: true }),
        );
      }
      
      rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error in setInnerHTML:", e);
      return false;
    }
  }

  /**
   * Sets the textContent of an element matching the given selector.
   *
   * @param selector - CSS selector for the target element
   * @param text - Text content to set
   * @returns boolean - true if successful, false otherwise
   */
  async setTextContent(selector: string, text: string): Promise<boolean> {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector) as HTMLElement | null;
      if (!element) {
        console.warn(
          `NRWebScraperChild: Element not found for setTextContent: ${selector}`,
        );
        return false;
      }

      this.scrollIntoViewIfNeeded(element);
      this.focusElementSoft(element);

      const elementInfo = await this.translate("inputValueSet", {
        value: this.truncate(text, 30),
      });
      const options = this.getHighlightOptions("Input");
      await this.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawDoc = (this.document as any)?.wrappedJSObject ?? this.document;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      // Attempt to use execCommand for better integration with rich text editors
      try {
        if (rawWin && rawDoc.hasFocus()) {
          const selection = rawWin.getSelection();
          if (selection) {
            const range = rawDoc.createRange();
            range.selectNodeContents(rawElement);
            selection.removeAllRanges();
            selection.addRange(range);

            if (rawDoc.execCommand("insertText", false, text)) {
               // Ensure events are still fired for safety
              rawElement.dispatchEvent(
                new EventCtor("input", { bubbles: true, cancelable: true }),
              );
              rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));
              return true;
            }
          }
        }
      } catch (e) {
        console.warn("NRWebScraperChild: execCommand failed, falling back:", e);
      }
      
      const InputEv = (rawWin?.InputEvent ?? null) as typeof InputEvent | null;

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv("beforeinput", {
            bubbles: true,
            cancelable: true,
            inputType: "insertText",
            data: text,
          }),
        );
      }

      if (rawElement !== element) {
         rawElement.textContent = text;
      } else {
         element.textContent = text;
      }

      // Dispatch input event to trigger any listeners
      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv("input", {
            bubbles: true,
            cancelable: false,
            inputType: "insertText",
            data: text,
          }),
        );
      } else {
        rawElement.dispatchEvent(
          new EventCtor("input", { bubbles: true, cancelable: true }),
        );
      }

      rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error in setTextContent:", e);
      return false;
    }
  }

  /**
   * Dispatches a custom event on an element matching the given selector.
   * Useful for triggering framework-specific event handlers.
   *
   * @param selector - CSS selector for the target element
   * @param eventType - Type of event to dispatch (e.g., "input", "change", "click")
   * @param options - Event options (bubbles, cancelable)
   * @returns boolean - true if successful, false otherwise
   */
  dispatchEvent(
    selector: string,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): boolean {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector);
      if (!element) {
        console.warn(
          `NRWebScraperChild: Element not found for dispatchEvent: ${selector}`,
        );
        return false;
      }

      const eventOptions = {
        bubbles: options?.bubbles ?? true,
        cancelable: options?.cancelable ?? true,
      };

      const event = new Event(eventType, eventOptions);
      element.dispatchEvent(event);

      return true;
    } catch (e) {
      console.error("NRWebScraperChild: Error in dispatchEvent:", e);
      return false;
    }
  }
}
