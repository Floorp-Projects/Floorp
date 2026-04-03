/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import type { ClickElementOptions } from "./types.ts";
import { unwrapElement, unwrapWindow, deepQuerySelector } from "./utils.ts";

const { setTimeout: timerSetTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/**
 * Interaction-oriented DOM utilities (click/hover/keys/drag)
 */
export class DOMActionOperations {
  constructor(private deps: DOMOpsDeps) {}

  private get contentWindow(): (Window & typeof globalThis) | null {
    return this.deps.getContentWindow();
  }

  private get document(): Document | null {
    return this.deps.getDocument();
  }

  /** querySelector that pierces shadow DOM */
  private deepQuery(selector: string): Element | null {
    const doc = this.document;
    return doc ? deepQuerySelector(doc, selector) : null;
  }

  /**
   * Playwright-style click with actionability checks and coordinate-based events.
   *
   * Steps:
   * 1. Find element
   * 2. Actionability check (visibility, size)
   * 3. Scroll into view
   * 4. Wait for position stability (CSS transitions)
   * 5. Fire coordinate-based mouse events via nsIDOMWindowUtils
   *
   * Falls back to DOM .click() if nsIDOMWindowUtils is unavailable.
   *
   * Note: hit-test via elementFromPoint is intentionally omitted — in Firefox
   * JSWindowActorChild (Xray context), elementFromPoint returns ancestor elements
   * due to a pointer-events Xray quirk. sendMouseEvent uses the native rendering
   * engine's hit-testing which correctly finds the target element.
   */
  async clickElement(
    selector: string,
    options: ClickElementOptions = {},
  ): Promise<boolean> {
    const {
      button = "left",
      clickCount = 1,
      force = false,
      timeout = 5000,
      stabilityTimeout = 100,
    } = options;
    const startTime = Date.now();
    const MAX_RETRIES = 3;

    for (let attempt = 0; attempt < MAX_RETRIES; attempt++) {
      if (Date.now() - startTime > timeout) return false;

      try {
        const el = this.deepQuery(selector) as HTMLElement | null;
        if (!el) {
          await this.delay(100);
          continue;
        }

        // Liveness check
        try {
          void el.nodeType;
        } catch {
          continue;
        }

        // Highlight
        const elementTagName = el.tagName?.toLowerCase() || "element";
        const elementTextRaw = el.textContent?.trim() || "";
        const truncatedText = this.deps.translationHelper.truncate(
          elementTextRaw,
          30,
        );
        const elementInfo =
          elementTextRaw.length > 0
            ? await this.deps.translationHelper.translate(
                "clickElementWithText",
                { tag: elementTagName, text: truncatedText },
              )
            : await this.deps.translationHelper.translate(
                "clickElementNoText",
                { tag: elementTagName },
              );
        const hlOpts = this.deps.highlightManager.getHighlightOptions("Click");
        this.deps.highlightManager
          .applyHighlight(el, hlOpts, elementInfo)
          .catch(() => {});

        if (!force) {
          // Actionability check
          if (!this.checkActionability(el)) {
            await this.delay(100);
            continue;
          }

          // Scroll into view
          el.scrollIntoView({ block: "center", behavior: "instant" as ScrollBehavior });
          await this.delay(50);

          // Wait for stable position (skip if stabilityTimeout is 0)
          if (stabilityTimeout > 0) {
            const remaining = Math.min(
              1000,
              timeout - (Date.now() - startTime),
            );
            const stable = await this.waitForStable(el, remaining, stabilityTimeout);
            if (!stable) continue;
          }

        }

        // Compute click coordinates once (after any stability wait)
        const rect = el.getBoundingClientRect();
        const x = rect.left + rect.width / 2;
        const y = rect.top + rect.height / 2;

        if (!force) {
          // Note: hit-test via elementFromPoint is intentionally omitted.
          // In Firefox JSWindowActorChild (Xray context), elementFromPoint returns
          // ancestor elements for interactive children (pointer-events quirk), and
          // Node.contains() comparisons across Xray/raw wrapper boundaries are
          // unreliable. The coordinate-based sendMouseEvent below uses the native
          // rendering engine's hit-testing which is not affected by this quirk.
        }

        // Attempt coordinate-based click via nsIDOMWindowUtils
        if (this.performMouseClick(x, y, button, clickCount)) {
          return true;
        }

        // Fallback: legacy DOM click path
        this.deps.eventDispatcher.scrollIntoViewIfNeeded(el);
        this.deps.eventDispatcher.focusElementSoft(el);
        this.deps.eventDispatcher.dispatchPointerClickSequence(el, x, y, 0);
        const rawElement = unwrapElement(
          el as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
        );
        try {
          rawElement.click();
        } catch {
          // ignore
        }
        return true;
      } catch (e) {
        console.error("DOMActionOperations: Error clicking element:", e);
        return false;
      }
    }

    return false;
  }

  /**
   * Fires a coordinate-based mouse event sequence via nsIDOMWindowUtils.
   * Returns true if successful, false if the API is unavailable.
   */
  private performMouseClick(
    x: number,
    y: number,
    button: "left" | "right" | "middle",
    clickCount: number,
  ): boolean {
    const domWindowUtils = (
      this.contentWindow as unknown as { windowUtils?: nsIDOMWindowUtils }
    )?.windowUtils;
    if (!domWindowUtils) return false;

    const buttonMap = { left: 0, middle: 1, right: 2 };
    const btn = buttonMap[button];

    try {
      // Move cursor to target
      domWindowUtils.sendMouseEvent("mousemove", x, y, 0, 0, 0);

      // mousedown → mouseup × clickCount
      for (let i = 0; i < clickCount; i++) {
        domWindowUtils.sendMouseEvent("mousedown", x, y, btn, i + 1, 0);
        domWindowUtils.sendMouseEvent("mouseup", x, y, btn, i + 1, 0);
      }
      return true;
    } catch (e) {
      console.error("DOMActionOperations: sendMouseEvent failed:", e);
      return false;
    }
  }

  /**
   * Playwright-style actionability check.
   */
  private checkActionability(el: HTMLElement): boolean {
    const rect = el.getBoundingClientRect();
    if (rect.width === 0 || rect.height === 0) return false;

    const style = this.contentWindow?.getComputedStyle(el);
    if (!style) return false;
    if (style.getPropertyValue("display") === "none") return false;
    if (style.getPropertyValue("visibility") === "hidden") return false;
    if (style.getPropertyValue("opacity") === "0") return false;
    // Note: pointer-events check omitted — getPropertyValue("pointer-events")
    // may return "none" for interactive elements in Firefox content process actors
    // even when the element is genuinely clickable (Xray wrapper/computed style quirk).

    return true;
  }

  /**
   * Waits until the element's position/size stabilizes (CSS animation done).
   * Uses a single before/after snapshot instead of polling to avoid Firefox's
   * background-tab interval throttling (which clamps setInterval to ~1000ms).
   */
  private async waitForStable(
    el: HTMLElement,
    timeout: number,
    delay: number = 100,
  ): Promise<boolean> {
    const initialRect = el.getBoundingClientRect();
    await new Promise<void>((resolve) =>
      timerSetTimeout(resolve, Math.min(delay, timeout)),
    );
    const finalRect = el.getBoundingClientRect();
    const dx = Math.abs(finalRect.x - initialRect.x);
    const dy = Math.abs(finalRect.y - initialRect.y);
    const dw = Math.abs(finalRect.width - initialRect.width);
    const dh = Math.abs(finalRect.height - initialRect.height);
    return dx < 2 && dy < 2 && dw < 2 && dh < 2;
  }

  private delay(ms: number): Promise<void> {
    return new Promise((resolve) => timerSetTimeout(resolve, ms));
  }

  async hoverElement(selector: string): Promise<boolean> {
    try {
      const element = this.deepQuery(selector) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "hoverElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Inspect");

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin) return false;

      const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent;
      const cloneEvInit = (opts: Record<string, unknown>) =>
        this.deps.eventDispatcher.cloneEventInit(opts);

      rawElement.dispatchEvent(
        new MouseEv(
          "mouseenter",
          cloneEvInit({
            bubbles: false,
            cancelable: false,
            clientX: centerX,
            clientY: centerY,
          }),
        ),
      );
      rawElement.dispatchEvent(
        new MouseEv(
          "mouseover",
          cloneEvInit({
            bubbles: true,
            cancelable: true,
            clientX: centerX,
            clientY: centerY,
          }),
        ),
      );
      rawElement.dispatchEvent(
        new MouseEv(
          "mousemove",
          cloneEvInit({
            bubbles: true,
            cancelable: true,
            clientX: centerX,
            clientY: centerY,
          }),
        ),
      );

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error hovering element:", e);
      return false;
    }
  }

  async scrollToElement(selector: string): Promise<boolean> {
    try {
      const element = this.deepQuery(selector) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "scrollToElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Inspect");

      element.scrollIntoView({ behavior: "smooth", block: "center" });

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error scrolling to element:", e);
      return false;
    }
  }

  doubleClickElement(selector: string): Promise<boolean> {
    return this.clickElement(selector, { clickCount: 2 });
  }

  rightClickElement(selector: string): Promise<boolean> {
    return this.clickElement(selector, { button: "right" });
  }

  async focusElement(selector: string): Promise<boolean> {
    try {
      const element = this.deepQuery(selector) as HTMLElement;
      if (!element) return false;

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "focusElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin) return false;

      const FocusEv = rawWin.FocusEvent ?? globalThis.FocusEvent;

      if (typeof rawElement.focus === "function") {
        rawElement.focus();
      } else {
        element.focus();
      }

      const cloneOpts = (opts: object) =>
        this.deps.eventDispatcher.cloneIntoPageContext(opts);

      rawElement.dispatchEvent(
        new FocusEv("focus", cloneOpts({ bubbles: false })),
      );
      rawElement.dispatchEvent(
        new FocusEv("focusin", cloneOpts({ bubbles: true })),
      );

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error focusing element:", e);
      return false;
    }
  }

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
      const rawWin = unwrapWindow(win);
      const activeRaw = active
        ? unwrapElement(
            active as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
          )
        : null;
      if (!rawWin) return false;

      const KeyboardEv = rawWin.KeyboardEvent ?? globalThis.KeyboardEvent;

      // Map logical key names to physical key codes
      const keyToCode = (k: string): string => {
        if (k.length === 1) {
          const upper = k.toUpperCase();
          if (upper >= "A" && upper <= "Z") return `Key${upper}`;
          if (k >= "0" && k <= "9") return `Digit${k}`;
          const special: Record<string, string> = {
            " ": "Space", ",": "Comma", ".": "Period", "/": "Slash",
            ";": "Semicolon", "'": "Quote", "[": "BracketLeft",
            "]": "BracketRight", "\\": "Backslash", "-": "Minus",
            "=": "Equal", "`": "Backquote",
          };
          return special[k] ?? k;
        }
        const multi: Record<string, string> = {
          Control: "ControlLeft", Shift: "ShiftLeft",
          Alt: "AltLeft", Meta: "MetaLeft",
        };
        return multi[k] ?? k;
      };

      // Compute modifier flags from the modifier key names
      const ctrlKey = modifiers.some((m) => m === "Control");
      const shiftKey = modifiers.some((m) => m === "Shift");
      const altKey = modifiers.some((m) => m === "Alt");
      const metaKey = modifiers.some((m) => m === "Meta");
      const modifierFlags = { ctrlKey, shiftKey, altKey, metaKey };

      const dispatch = (type: string, opts: KeyboardEventInit) => {
        try {
          return (
            activeRaw?.dispatchEvent(
              new KeyboardEv(
                type,
                this.deps.eventDispatcher.cloneEventInit(
                  opts as Record<string, unknown>,
                ),
              ),
            ) ?? false
          );
        } catch {
          return false;
        }
      };

      for (const mod of modifiers) {
        dispatch("keydown", { key: mod, code: keyToCode(mod), bubbles: true, ...modifierFlags });
      }

      dispatch("keydown", { key, code: keyToCode(key), bubbles: true, ...modifierFlags });
      dispatch("keypress", { key, code: keyToCode(key), bubbles: true, ...modifierFlags });
      dispatch("keyup", { key, code: keyToCode(key), bubbles: true, ...modifierFlags });

      for (const mod of [...modifiers].reverse()) {
        dispatch("keyup", { key: mod, code: keyToCode(mod), bubbles: true, ...modifierFlags });
      }

      await Promise.resolve();
      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error pressing key:", e);
      return false;
    }
  }

  async dragAndDrop(
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean> {
    try {
      const source = this.deepQuery(sourceSelector) as HTMLElement;
      const target = this.deepQuery(targetSelector) as HTMLElement;

      if (!source || !target) return false;

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(source);
      this.deps.eventDispatcher.scrollIntoViewIfNeeded(target);

      const elementInfo = await this.deps.translationHelper.translate(
        "dragAndDrop",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      this.deps.highlightManager
        .applyHighlight(source, options, elementInfo)
        .catch(() => {});
      this.deps.highlightManager
        .applyHighlight(target, options, elementInfo)
        .catch(() => {});

      const sourceRect = source.getBoundingClientRect();
      const targetRect = target.getBoundingClientRect();
      const sourceX = sourceRect.left + sourceRect.width / 2;
      const sourceY = sourceRect.top + sourceRect.height / 2;
      const targetX = targetRect.left + targetRect.width / 2;
      const targetY = targetRect.top + targetRect.height / 2;

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawSource = unwrapElement(
        source as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      const rawTarget = unwrapElement(
        target as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin) return false;

      const DragEv = rawWin.DragEvent ?? globalThis.DragEvent;
      const DataTransferCtor = rawWin.DataTransfer ?? globalThis.DataTransfer;

      const dataTransfer = new DataTransferCtor();

      // Clone serializable properties, then re-attach dataTransfer (non-clonable DOM object)
      const makeDragInit = (serializable: Record<string, unknown>) => {
        const cloned = this.deps.eventDispatcher.cloneEventInit(serializable);
        (cloned as Record<string, unknown>).dataTransfer = dataTransfer;
        return cloned;
      };

      rawSource.dispatchEvent(
        new DragEv(
          "dragstart",
          makeDragInit({
            bubbles: true,
            cancelable: true,
            clientX: sourceX,
            clientY: sourceY,
          }),
        ),
      );

      rawSource.dispatchEvent(
        new DragEv(
          "drag",
          makeDragInit({
            bubbles: true,
            cancelable: true,
            clientX: sourceX,
            clientY: sourceY,
          }),
        ),
      );

      rawTarget.dispatchEvent(
        new DragEv(
          "dragenter",
          makeDragInit({
            bubbles: true,
            cancelable: true,
            clientX: targetX,
            clientY: targetY,
          }),
        ),
      );

      rawTarget.dispatchEvent(
        new DragEv(
          "dragover",
          makeDragInit({
            bubbles: true,
            cancelable: true,
            clientX: targetX,
            clientY: targetY,
          }),
        ),
      );

      rawTarget.dispatchEvent(
        new DragEv(
          "drop",
          makeDragInit({
            bubbles: true,
            cancelable: true,
            clientX: targetX,
            clientY: targetY,
          }),
        ),
      );

      rawSource.dispatchEvent(
        new DragEv(
          "dragend",
          makeDragInit({
            bubbles: true,
            cancelable: false,
            clientX: targetX,
            clientY: targetY,
          }),
        ),
      );

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error in drag and drop:", e);
      return false;
    }
  }
}
