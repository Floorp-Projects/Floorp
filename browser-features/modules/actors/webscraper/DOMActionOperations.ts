/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import type {
  ClickElementOptions,
  ClickElementResult,
  ClickStabilityAnchor,
  PrivilegedMouseWindow,
} from "./types.ts";
import {
  deepQuerySelector,
  deepQuerySelectorAll,
  unwrapElement,
  unwrapWindow,
} from "./utils.ts";

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

  /** Wait for one actionable target, then dispatch one native mouse sequence. */
  async clickElement(
    selector: string,
    options: ClickElementOptions = {},
  ): Promise<boolean> {
    return (await this.clickElementWithResult(selector, options)).ok;
  }

  async clickElementWithResult(
    selector: string,
    options: ClickElementOptions = {},
  ): Promise<ClickElementResult> {
    const result: ClickElementResult = {
      ok: false,
      status: "refused",
      reason: "deadline-exceeded",
      phase: "prepare",
      inputStarted: false,
      activationStarted: false,
      backend: "window-synthesizeMouseEvent",
    };
    const finish = (status: ClickElementResult["status"], reason: string) => {
      result.status = status;
      result.reason = reason;
      result.ok = status === "dispatched";
      return result;
    };
    try {
      const {
        button = "left",
        clickCount = 1,
        force = false,
        timeout = 5000,
        stabilityTimeout = 100,
      } = options;
      const win = this.contentWindow;
      const doc = this.document;
      if (
        !Number.isFinite(timeout) || timeout < 0 ||
        !Number.isFinite(stabilityTimeout) || stabilityTimeout < 0 ||
        !Number.isInteger(clickCount) || clickCount < 1 || clickCount > 2 ||
        !["left", "right", "middle"].includes(button) ||
        typeof force !== "boolean"
      ) {
        return finish("refused", "invalid-options");
      }
      if (!win || !doc) return finish("refused", "document-unavailable");
      const mouseWindow = win as unknown as PrivilegedMouseWindow;
      if (typeof mouseWindow.synthesizeMouseEvent !== "function") {
        return finish("unsupported", "native-input-unavailable");
      }
      const now = () => win.performance.now();
      const deadline = now() + Math.min(timeout, 60_000);
      let scrolled: Element | null = null;
      let anchor: ClickStabilityAnchor | null = null;

      result.phase = "wait";
      while (now() < deadline) {
        if (this.document !== doc || this.contentWindow !== win) {
          return finish("refused", "document-changed");
        }
        const matches = deepQuerySelectorAll(doc, selector);
        if (matches.length > 1) return finish("refused", "ambiguous-selector");
        const el = matches[0];
        if (
          !el || !el.isConnected || el.ownerDocument !== doc ||
          (!force && !this.checkActionability(el))
        ) {
          anchor = null;
          await this.delay(Math.min(25, Math.max(0, deadline - now())));
          continue;
        }
        if (!force && scrolled !== el) {
          el.scrollIntoView({ block: "center", behavior: "instant" });
          scrolled = el;
          anchor = null;
        }
        const rect = el.getBoundingClientRect();
        if (!force && !this.receivesPointerEvents(el, rect)) {
          anchor = null;
          await this.delay(Math.min(25, Math.max(0, deadline - now())));
          continue;
        }
        if (!force && stabilityTimeout > 0) {
          if (
            !anchor || anchor.element !== el ||
            !this.sameRect(anchor.rect, rect)
          ) {
            anchor = { element: el, rect, at: now() };
          }
          if (now() - anchor.at < stabilityTimeout) {
            await this.delay(Math.min(25, Math.max(0, deadline - now())));
            continue;
          }
        }
        const recheck = () => {
          if (
            now() >= deadline || this.document !== doc ||
            this.contentWindow !== win ||
            !el.isConnected || el.ownerDocument !== doc
          ) return false;
          const current = deepQuerySelectorAll(doc, selector);
          return current.length === 1 && current[0] === el &&
            (force || (this.checkActionability(el) &&
              this.sameRect(rect, el.getBoundingClientRect()) &&
              this.receivesPointerEvents(el, rect))) && now() < deadline;
        };
        if (!recheck()) {
          anchor = null;
          await this.delay(Math.min(25, Math.max(0, deadline - now())));
          continue;
        }
        const sent = this.deps.highlightManager.withControlOverlaySuspended(() =>
          this.performMouseClick(
            mouseWindow,
            rect.x + rect.width / 2,
            rect.y + rect.height / 2,
            button,
            clickCount,
            recheck,
            result,
          )
        );
        if (sent.ok) {
          void (async () => {
            if (!el.isConnected) return;
            const tag = el.tagName?.toLowerCase() || "element";
            const text = this.deps.translationHelper.truncate(
              el.textContent?.trim() || "",
              30,
            );
            const info = await this.deps.translationHelper.translate(
              text ? "clickElementWithText" : "clickElementNoText",
              text ? { tag, text } : { tag },
            );
            if (!el.isConnected) return;
            await this.deps.highlightManager.applyHighlight(
              el,
              this.deps.highlightManager.getHighlightOptions("Click"),
              info,
            );
          })().catch(() => {});
        }
        return sent;
      }
    } catch (e) {
      console.error("[DOMActionOperations] Error clicking element:", e);
      return finish("unknown", `click-error: ${String(e)}`);
    }
    return finish("refused", "deadline-exceeded");
  }

  private performMouseClick(
    win: PrivilegedMouseWindow,
    x: number,
    y: number,
    button: "left" | "right" | "middle",
    clickCount: number,
    recheck: () => boolean,
    result: ClickElementResult,
  ): ClickElementResult {
    if (typeof win.synthesizeMouseEvent !== "function") {
      return {
        ...result,
        status: "unsupported",
        reason: "native-input-unavailable",
      };
    }
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      return { ...result, status: "refused", reason: "invalid-coordinates" };
    }
    const buttonMap = { left: 0, middle: 1, right: 2 };
    const btn = buttonMap[button];
    const buttons = { left: 1, middle: 4, right: 2 }[button];
    const options = { toWindow: true, isAsyncEnabled: false };

    try {
      result.phase = "mousemove";
      result.inputStarted = true;
      win.synthesizeMouseEvent("mousemove", x, y, {
        button: btn,
        buttons: 0,
        clickCount: 0,
      }, options);
      for (let i = 0; i < clickCount; i++) {
        if (!recheck()) {
          return {
            ...result,
            status: result.activationStarted ? "unknown" : "refused",
            reason: result.activationStarted
              ? "eligibility-changed-after-activation"
              : "eligibility-changed-before-mousedown",
          };
        }
        result.phase = "mousedown";
        result.activationStarted = true;
        win.synthesizeMouseEvent("mousedown", x, y, {
          button: btn,
          buttons,
          clickCount: i + 1,
        }, options);
        result.phase = "mouseup";
        win.synthesizeMouseEvent("mouseup", x, y, {
          button: btn,
          buttons: 0,
          clickCount: i + 1,
        }, options);
      }
      return {
        ...result,
        ok: true,
        status: "dispatched",
        reason: "native-sequence-complete",
        phase: "complete",
      };
    } catch (e) {
      console.error("[DOMActionOperations] Native mouse delivery failed:", e);
      return {
        ...result,
        status: "unknown",
        reason: `native-input-error: ${String(e)}`,
      };
    }
  }

  private checkActionability(el: Element): boolean {
    const rect = el.getBoundingClientRect();
    if (
      rect.width <= 0 || rect.height <= 0 || el.matches(":disabled") ||
      el.closest('[aria-disabled="true"]')
    ) return false;

    const style = this.contentWindow?.getComputedStyle(el);
    if (!style) return false;
    if (style.getPropertyValue("display") === "none") return false;
    if (style.getPropertyValue("visibility") === "hidden") return false;
    if (style.getPropertyValue("opacity") === "0") return false;
    return true;
  }

  private receivesPointerEvents(el: Element, rect: DOMRect): boolean {
    return this.deps.highlightManager.withControlOverlaySuspended(() =>
      this.hitTestTarget(el, rect)
    );
  }

  private hitTestTarget(el: Element, rect: DOMRect): boolean {
    const x = rect.x + rect.width / 2;
    const y = rect.y + rect.height / 2;
    if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
    // Document hit testing retargets shadow descendants to their host. Walk
    // through the hit hosts, preserving the outer hit test so an overlay
    // outside a shadow root still blocks the target.
    let hit = this.document?.elementFromPoint(x, y) ?? null;
    while (hit?.shadowRoot) {
      const inner = hit.shadowRoot.elementFromPoint(x, y);
      if (!inner || inner === hit) break;
      hit = inner;
    }
    while (hit) {
      if (el.contains(hit)) return true;
      const root = hit.getRootNode();
      hit = hit.assignedSlot ?? hit.parentElement ??
        (root.nodeType === 11 && "host" in root
          ? (root as ShadowRoot).host
          : null);
    }
    return false;
  }

  private sameRect(a: DOMRect, b: DOMRect): boolean {
    return [a.x - b.x, a.y - b.y, a.width - b.width, a.height - b.height]
      .every((difference) => Math.abs(difference) < 2);
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
