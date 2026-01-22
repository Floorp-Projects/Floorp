/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import { unwrapElement, unwrapWindow } from "./utils.ts";

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

  async clickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return Promise.resolve(false);

      const elementTagName = element.tagName?.toLowerCase() || "element";
      const elementTextRaw = element.textContent?.trim() || "";
      const truncatedText = this.deps.translationHelper.truncate(
        elementTextRaw,
        30,
      );
      const elementInfo =
        elementTextRaw.length > 0
          ? await this.deps.translationHelper.translate(
              "clickElementWithText",
              {
                tag: elementTagName,
                text: truncatedText,
              },
            )
          : await this.deps.translationHelper.translate("clickElementNoText", {
              tag: elementTagName,
            });

      try {
        void element.nodeType;
      } catch {
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const options = this.deps.highlightManager.getHighlightOptions("Click");

      const effectPromise = this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

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
          console.warn(
            "DOMActionOperations: input/change dispatch failed",
            err,
          );
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

      let clickDispatched =
        this.deps.eventDispatcher.dispatchPointerClickSequence(
          element,
          centerX,
          centerY,
          0,
        );

      try {
        const rawElement = unwrapElement(
          element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
        );
        rawElement.click();
        clickDispatched = true;
      } catch {
        // ignore
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
          console.warn("DOMActionOperations: synthetic click failed", err);
        }
      }

      if ((isButton || isLink) && !clickDispatched) {
        try {
          element.dispatchEvent(
            new Ev("submit", { bubbles: true, cancelable: true }),
          );
        } catch (err) {
          console.warn("DOMActionOperations: submit dispatch failed", err);
        }
      }

      const success = stateChanged || clickDispatched;

      await Promise.race([
        effectPromise,
        new Promise((resolve) => timerSetTimeout(resolve, 300)),
      ]);

      return success;
    } catch (e) {
      console.error("DOMActionOperations: Error clicking element:", e);
      return Promise.resolve(false);
    }
  }

  async hoverElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "hoverElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Inspect");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

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
      console.error("DOMActionOperations: Error hovering element:", e);
      return false;
    }
  }

  async scrollToElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "scrollToElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Inspect");

      element.scrollIntoView({ behavior: "smooth", block: "center" });

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error scrolling to element:", e);
      return false;
    }
  }

  async doubleClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "doubleClickElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Click");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      this.deps.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );
      this.deps.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin) return false;

      const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent;

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
      console.error("DOMActionOperations: Error double clicking element:", e);
      return false;
    }
  }

  async rightClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "rightClickElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Click");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      this.deps.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        2,
      );

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin) return false;

      const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent;

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
      console.error("DOMActionOperations: Error right clicking element:", e);
      return false;
    }
  }

  async focusElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as HTMLElement;
      if (!element) return false;

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "focusElement",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

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
      console.error("DOMActionOperations: Error pressing key:", e);
      return false;
    }
  }

  async dragAndDrop(
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean> {
    try {
      const source = this.document?.querySelector(
        sourceSelector,
      ) as HTMLElement;
      const target = this.document?.querySelector(
        targetSelector,
      ) as HTMLElement;

      if (!source || !target) return false;

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(source);
      this.deps.eventDispatcher.scrollIntoViewIfNeeded(target);

      const elementInfo = await this.deps.translationHelper.translate(
        "dragAndDrop",
        {},
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      await this.deps.highlightManager.applyHighlight(
        source,
        options,
        elementInfo,
      );
      await this.deps.highlightManager.applyHighlight(
        target,
        options,
        elementInfo,
      );

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
          clientX: targetX,
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      return true;
    } catch (e) {
      console.error("DOMActionOperations: Error in drag and drop:", e);
      return false;
    }
  }
}
