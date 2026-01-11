/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * EventDispatcher - Utilities for dispatching DOM events
 */

import type {
  ContentWindow,
  RawContentWindow,
  WebScraperContext,
  XrayElement,
  XrayHTMLElement,
  XrayInputElement,
  XrayInputLikeElement,
  XraySelectElement,
} from "./types.ts";

/**
 * Helper to unwrap Xray-wrapped window
 */
function unwrapWindow(win: ContentWindow | null): RawContentWindow | null {
  if (!win) return null;
  return win.wrappedJSObject ?? (win as unknown as RawContentWindow);
}

/**
 * Helper to unwrap Xray-wrapped element
 */
function unwrapElement<T extends Element>(
  element: T & Partial<{ wrappedJSObject: T }>,
): T {
  return element.wrappedJSObject ?? element;
}

/**
 * Helper class for dispatching DOM events with proper context handling
 */
export class EventDispatcher {
  constructor(private context: WebScraperContext) {}

  get contentWindow(): ContentWindow | null {
    return this.context.contentWindow as ContentWindow | null;
  }

  /**
   * Dispatches a pointer + mouse click sequence for better framework compatibility
   */
  dispatchPointerClickSequence(
    element: XrayHTMLElement,
    clientX: number,
    clientY: number,
    button: number,
  ): boolean {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    const rawElement = unwrapElement(element);

    if (!rawWin) return false;

    const PointerEv = rawWin.PointerEvent ?? null;
    const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent ?? null;
    const view = rawWin;

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
  focusElementSoft(element: XrayElement): void {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    if (!rawWin) return;

    try {
      const rawElement = unwrapElement(element) as HTMLElement;
      const FocusEv = rawWin.FocusEvent ?? globalThis.FocusEvent;

      if (typeof rawElement.focus === "function") {
        // Clone focus options into content context to avoid security wrapper issues
        const focusOpts = Cu.cloneInto({ preventScroll: true }, rawWin);
        rawElement.focus(focusOpts);
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
    element: XrayElement,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): boolean {
    try {
      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(element);

      if (!rawWin) return false;

      const EventCtor = rawWin.Event ?? globalThis.Event;

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
  dispatchInputEvents(element: XrayElement): void {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    if (!rawWin) return;

    const rawElement = unwrapElement(element);
    const EventCtor = rawWin.Event ?? globalThis.Event;
    const FocusEv = rawWin.FocusEvent ?? globalThis.FocusEvent;

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
    element: XrayInputLikeElement,
  ): ((value: string) => void) | undefined {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    if (!rawWin) return undefined;

    // Handle union type manually to avoid type inference issues
    const rawElement: HTMLInputElement | HTMLTextAreaElement =
      element.wrappedJSObject ?? element;

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
    element: XrayInputElement,
  ): ((checked: boolean) => void) | undefined {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    if (!rawWin) return undefined;

    const rawElement = unwrapElement(element);

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
    element: XraySelectElement,
  ): ((value: string) => void) | undefined {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    if (!rawWin) return undefined;

    const rawElement = unwrapElement(element);

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
