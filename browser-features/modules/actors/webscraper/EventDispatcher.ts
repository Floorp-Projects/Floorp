/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * EventDispatcher - Utilities for dispatching DOM events
 */

import type { WebScraperContext } from "./types.ts";

/**
 * Helper class for dispatching DOM events with proper context handling
 */
export class EventDispatcher {
  constructor(private context: WebScraperContext) {}

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  /**
   * Dispatches a pointer + mouse click sequence for better framework compatibility
   */
  dispatchPointerClickSequence(
    element: HTMLElement,
    clientX: number,
    clientY: number,
    button: number,
  ): boolean {
    const win = this.contentWindow;
    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;

    const PointerEv = (rawWin?.PointerEvent ?? null) as
      | typeof PointerEvent
      | null;
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

  /**
   * Focus helper that avoids scroll jumps
   */
  focusElementSoft(element: Element): void {
    const win = this.contentWindow;
    if (!win) return;
    try {
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any).wrappedJSObject ?? element;
      const htmlEl = rawElement as unknown as HTMLElement;

      const FocusEv = (rawWin?.FocusEvent ??
        globalThis.FocusEvent) as typeof FocusEvent;

      if (typeof htmlEl.focus === "function") {
        htmlEl.focus({ preventScroll: true });
      }

      try {
        rawElement.dispatchEvent(new FocusEv("focus", { bubbles: false }));
        rawElement.dispatchEvent(new FocusEv("focusin", { bubbles: true }));
      } catch {
        // ignore event dispatch errors
      }
    } catch {
      // ignore focus errors
    }
  }

  /**
   * Ensures the target element is in view
   */
  scrollIntoViewIfNeeded(element: Element): void {
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
   * Clone event options into page context using Cu.cloneInto
   */
  cloneIntoPageContext(opts: object): object {
    const win = this.contentWindow;
    try {
      if (typeof Cu?.cloneInto === "function" && win) {
        return Cu.cloneInto(opts, win);
      }
    } catch {
      // Fallback if cloneInto not available
    }
    return opts;
  }

  /**
   * Dispatch a custom event on an element
   */
  dispatchCustomEvent(
    element: Element,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): boolean {
    try {
      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      const eventOptions = {
        bubbles: options?.bubbles ?? true,
        cancelable: options?.cancelable ?? true,
      };

      const event = new EventCtor(eventType, eventOptions);
      rawElement.dispatchEvent(event);

      return true;
    } catch (e) {
      console.error("EventDispatcher: Error dispatching event:", e);
      return false;
    }
  }

  /**
   * Dispatch input and change events for form elements
   */
  dispatchInputEvents(element: Element): void {
    const win = this.contentWindow;
    if (!win) return;

    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;
    const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;
    const FocusEv = (rawWin?.FocusEvent ??
      globalThis.FocusEvent) as typeof FocusEvent;

    try {
      rawElement.dispatchEvent(
        new EventCtor("input", this.cloneIntoPageContext({ bubbles: true })),
      );
      rawElement.dispatchEvent(
        new EventCtor("change", this.cloneIntoPageContext({ bubbles: true })),
      );
      rawElement.dispatchEvent(
        new FocusEv("blur", this.cloneIntoPageContext({ bubbles: true })),
      );
    } catch {
      // ignore event errors
    }
  }

  /**
   * Get native value setter for React compatibility
   */
  getNativeValueSetter(
    element: HTMLInputElement | HTMLTextAreaElement,
  ): ((value: string) => void) | undefined {
    const win = this.contentWindow;
    if (!win) return undefined;

    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;

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
      return (value: string) => setter.call(rawElement, value);
    }

    return (value: string) => {
      rawElement.value = value;
    };
  }

  /**
   * Get native checked setter for React compatibility
   */
  getNativeCheckedSetter(
    element: HTMLInputElement,
  ): ((checked: boolean) => void) | undefined {
    const win = this.contentWindow;
    if (!win) return undefined;

    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;

    const HTMLInputProto = rawWin.HTMLInputElement?.prototype;
    const nativeCheckedSetter = HTMLInputProto
      ? Object.getOwnPropertyDescriptor(HTMLInputProto, "checked")?.set
      : undefined;

    if (nativeCheckedSetter) {
      return (checked: boolean) =>
        nativeCheckedSetter.call(rawElement, checked);
    }

    return (checked: boolean) => {
      rawElement.checked = checked;
    };
  }

  /**
   * Get native select value setter for React compatibility
   */
  getNativeSelectValueSetter(
    element: HTMLSelectElement,
  ): ((value: string) => void) | undefined {
    const win = this.contentWindow;
    if (!win) return undefined;

    const rawWin = (win as any)?.wrappedJSObject ?? win;
    const rawElement = (element as any)?.wrappedJSObject ?? element;

    const HTMLSelectProto = rawWin.HTMLSelectElement?.prototype;
    const nativeSelectValueSetter = HTMLSelectProto
      ? Object.getOwnPropertyDescriptor(HTMLSelectProto, "value")?.set
      : undefined;

    if (nativeSelectValueSetter) {
      return (value: string) => nativeSelectValueSetter.call(rawElement, value);
    }

    return (value: string) => {
      rawElement.value = value;
    };
  }
}
