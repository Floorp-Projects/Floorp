/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import type { RawContentWindow } from "./types.ts";
import { unwrapDocument, unwrapElement, unwrapWindow } from "./utils.ts";

const { setTimeout: timerSetTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/**
 * Write/input focused DOM utilities
 */
export class DOMWriteOperations {
  constructor(private deps: DOMOpsDeps) {}

  private get contentWindow(): (Window & typeof globalThis) | null {
    return this.deps.getContentWindow();
  }

  private get document(): Document | null {
    return this.deps.getDocument();
  }

  private tryExecCommand(
    rawWin: RawContentWindow,
    rawDoc: Document,
    rawElement: HTMLElement,
    command: "insertHTML" | "insertText",
    value: string,
  ): boolean {
    try {
      if (!rawDoc.hasFocus()) {
        return false;
      }
      const selection = rawWin.getSelection();
      if (!selection) {
        return false;
      }
      const range = rawDoc.createRange();
      range.selectNodeContents(rawElement);
      selection.removeAllRanges();
      selection.addRange(range);

      if (rawDoc.execCommand(command, false, value)) {
        const EventCtor = rawWin.Event ?? globalThis.Event;
        rawElement.dispatchEvent(
          new EventCtor("input", { bubbles: true, cancelable: true }),
        );
        rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));
        return true;
      }
    } catch (e) {
      console.warn("DOMWriteOperations: execCommand failed, falling back:", e);
    }
    return false;
  }

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

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      if (!options.skipHighlight) {
        const truncatedValue = this.deps.translationHelper.truncate(value, 50);
        const elementInfo = await this.deps.translationHelper.translate(
          "inputValueSet",
          {
            value: truncatedValue,
          },
        );
        const highlightOptions =
          this.deps.highlightManager.getHighlightOptions("Input");

        await this.deps.highlightManager.applyHighlight(
          element,
          highlightOptions,
          elementInfo,
        );
      }

      const win = this.contentWindow;
      if (!win) return false;

      try {
        void element.nodeType;
      } catch {
        return false;
      }

      if (element instanceof win.HTMLSelectElement) {
        return this.selectOption(selector, value);
      }

      const setter = this.deps.eventDispatcher.getNativeValueSetter(
        element as HTMLInputElement | HTMLTextAreaElement,
      );

      const typingMode = options.typingMode === true;
      const typingDelay =
        typeof options.typingDelayMs === "number"
          ? Math.max(0, options.typingDelayMs)
          : 25;

      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as HTMLInputElement &
          Partial<{ wrappedJSObject: HTMLInputElement }>,
      );
      if (!rawWin) return false;

      const InputEv = rawWin.InputEvent ?? null;
      const EventCtor = rawWin.Event ?? globalThis.Event;
      const KeyboardEv = rawWin.KeyboardEvent ?? globalThis.KeyboardEvent;
      const FocusEv = rawWin.FocusEvent ?? globalThis.FocusEvent;

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

      const setValue = setter
        ? (v: string) => setter(v)
        : (v: string) => {
            rawElement.value = v;
          };

      const cloneOpts = (opts: object) =>
        this.deps.eventDispatcher.cloneIntoPageContext(opts);

      if (typingMode) {
        setValue("");
        for (const ch of value.split("")) {
          rawElement.dispatchEvent(
            new KeyboardEv("keydown", cloneOpts({ key: ch, bubbles: true })),
          );
          dispatchBeforeInput(ch);
          setValue(rawElement.value + ch);
          rawElement.dispatchEvent(
            new EventCtor("input", cloneOpts({ bubbles: true })),
          );
          rawElement.dispatchEvent(
            new KeyboardEv("keyup", cloneOpts({ key: ch, bubbles: true })),
          );
          if (typingDelay > 0) {
            await new Promise((r) => timerSetTimeout(r, typingDelay));
          }
        }
        rawElement.dispatchEvent(
          new EventCtor("change", cloneOpts({ bubbles: true })),
        );
      } else {
        dispatchBeforeInput(value);
        setValue(value);
        rawElement.dispatchEvent(
          new EventCtor("input", cloneOpts({ bubbles: true })),
        );
        rawElement.dispatchEvent(
          new EventCtor("change", cloneOpts({ bubbles: true })),
        );
      }

      rawElement.dispatchEvent(
        new FocusEv("blur", cloneOpts({ bubbles: true })),
      );
      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error setting input value:", e);
      return false;
    }
  }

  async clearInput(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (!element) return false;

      if (!("value" in element)) return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "inputValueSet",
        {
          value: "(cleared)",
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      const win = this.contentWindow;
      if (!win) return false;

      const setter = this.deps.eventDispatcher.getNativeValueSetter(element);
      if (setter) {
        setter("");
      } else {
        element.value = "";
      }

      this.deps.eventDispatcher.dispatchInputEvents(element);

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error clearing input:", e);
      return false;
    }
  }

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

      if (element.tagName !== "SELECT") return false;

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
        const elementInfo = await this.deps.translationHelper.translate(
          "selectOption",
          {
            value: targetOpt.value,
          },
        );
        const highlightOptions =
          this.deps.highlightManager.getHighlightOptions("Input");

        await this.deps.highlightManager.applyHighlight(
          element,
          highlightOptions,
          elementInfo,
        );
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const setter =
        this.deps.eventDispatcher.getNativeSelectValueSetter(element);
      if (setter) {
        setter(targetOpt.value);
      } else {
        element.value = targetOpt.value;
      }

      this.deps.eventDispatcher.dispatchInputEvents(element);

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error selecting option:", e);
      return false;
    }
  }

  async setChecked(selector: string, checked: boolean): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element) return false;

      if (element.tagName !== "INPUT") return false;
      if (element.type !== "checkbox" && element.type !== "radio") return false;

      const elementInfo = await this.deps.translationHelper.translate(
        "setChecked",
        {
          state: checked ? "checked" : "unchecked",
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Click");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const setter = this.deps.eventDispatcher.getNativeCheckedSetter(element);
      if (setter) {
        setter(checked);
      } else {
        element.checked = checked;
      }

      this.deps.eventDispatcher.dispatchInputEvents(element);

      if (element.type === "radio" && checked) {
        const win = this.contentWindow;
        const rawWin = unwrapWindow(win);
        const rawElement = unwrapElement(
          element as HTMLInputElement &
            Partial<{ wrappedJSObject: HTMLInputElement }>,
        );
        if (rawWin) {
          const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent;
          rawElement.dispatchEvent(new MouseEv("click", { bubbles: true }));
        }
      }

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error setting checked state:", e);
      return false;
    }
  }

  async uploadFile(selector: string, filePath: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element || element.tagName !== "INPUT" || element.type !== "file") {
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "uploadFile",
        {
          value: this.deps.translationHelper.truncate(
            filePath.split(/[\\/]/).pop() ?? filePath,
            30,
          ),
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");
      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      type MozFileInput = HTMLInputElement & {
        mozSetFileArray?: (files: File[]) => void;
      };
      const fileInput = element as MozFileInput;

      try {
        const file = await File.createFromFileName(filePath);
        if (typeof fileInput.mozSetFileArray === "function") {
          fileInput.mozSetFileArray([file]);
        } else {
          console.error("DOMWriteOperations: mozSetFileArray not available");
          return false;
        }
      } catch (e) {
        console.error(
          "DOMWriteOperations: Failed to create file from path:",
          e,
        );
        return false;
      }

      element.dispatchEvent(new Event("input", { bubbles: true }));
      element.dispatchEvent(new Event("change", { bubbles: true }));
      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error uploading file:", e);
      return false;
    }
  }

  setCookieString(
    cookieString: string,
    cookieName?: string,
    cookieValue?: string,
  ): boolean {
    try {
      const win = this.contentWindow;
      if (!win?.document) {
        return false;
      }

      win.document.cookie = cookieString;

      if (cookieName && cookieValue) {
        const cookies = win.document.cookie;
        return cookies.includes(`${cookieName}=${cookieValue}`);
      }

      return true;
    } catch {
      return false;
    }
  }

  async setInnerHTML(selector: string, html: string): Promise<boolean> {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector) as HTMLElement | null;
      if (!element) {
        console.warn(
          `DOMWriteOperations: Element not found for setInnerHTML: ${selector}`,
        );
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "inputValueSet",
        {
          value: this.deps.translationHelper.truncate(html, 30),
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");

      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawDoc = this.document
        ? unwrapDocument(
            this.document as Document & Partial<{ wrappedJSObject: Document }>,
          )
        : null;
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin || !rawDoc) return false;

      if (this.tryExecCommand(rawWin, rawDoc, rawElement, "insertHTML", html)) {
        return true;
      }

      const EventCtor = rawWin.Event ?? globalThis.Event;
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

      if (rawElement !== element) {
        rawElement.innerHTML = html;
      } else {
        element.innerHTML = html;
      }

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
      console.error("DOMWriteOperations: Error in setInnerHTML:", e);
      return false;
    }
  }

  async setTextContent(selector: string, text: string): Promise<boolean> {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector) as HTMLElement | null;
      if (!element) {
        console.warn(
          `DOMWriteOperations: Element not found for setTextContent: ${selector}`,
        );
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "inputValueSet",
        {
          value: this.deps.translationHelper.truncate(text, 30),
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");
      await this.deps.highlightManager.applyHighlight(
        element,
        options,
        elementInfo,
      );

      const win = this.contentWindow;
      const rawWin = unwrapWindow(win);
      const rawDoc = this.document
        ? unwrapDocument(
            this.document as Document & Partial<{ wrappedJSObject: Document }>,
          )
        : null;
      const rawElement = unwrapElement(
        element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
      );
      if (!rawWin || !rawDoc) return false;

      const EventCtor = rawWin.Event ?? globalThis.Event;

      if (this.tryExecCommand(rawWin, rawDoc, rawElement, "insertText", text)) {
        rawElement.dispatchEvent(
          new EventCtor("input", { bubbles: true, cancelable: true }),
        );
        rawElement.dispatchEvent(new EventCtor("change", { bubbles: true }));
        return true;
      }

      const InputEv = rawWin.InputEvent ?? null;

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
      console.error("DOMWriteOperations: Error in setTextContent:", e);
      return false;
    }
  }
}
