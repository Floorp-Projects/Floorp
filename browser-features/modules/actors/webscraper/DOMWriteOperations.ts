/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import type { RawContentWindow } from "./types.ts";
import {
  deepQuerySelector,
  unwrapDocument,
  unwrapElement,
  unwrapWindow,
} from "./utils.ts";

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

  private deepQuery(selector: string): Element | null {
    const doc = this.document;
    return doc ? deepQuerySelector(doc, selector) : null;
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
        const cloneOpts = (opts: object) =>
          this.deps.eventDispatcher.cloneIntoPageContext(opts);
        rawElement.dispatchEvent(
          new EventCtor(
            "input",
            cloneOpts({ bubbles: true, cancelable: true }),
          ),
        );
        rawElement.dispatchEvent(
          new EventCtor("change", cloneOpts({ bubbles: true })),
        );
        return true;
      }
    } catch (e) {
      console.warn("DOMWriteOperations: execCommand failed, falling back:", e);
    }
    return false;
  }

  /**
   * Common context setup for DOM write operations.
   * Unwraps Xray security wrappers and prepares event constructors
   * shared by setInnerHTML, setTextContent, and dispatchTextInput.
   */
  private getWriteContext(element: HTMLElement): {
    rawWin: RawContentWindow;
    rawDoc: Document;
    rawElement: HTMLElement;
    InputEv: typeof InputEvent | null;
    EventCtor: typeof Event;
    cloneOpts: (opts: object) => object;
  } | null {
    const win = this.contentWindow;
    const rawWin = unwrapWindow(win);
    const rawDoc = this.document
      ? unwrapDocument(
        this.document as Document & Partial<{ wrappedJSObject: Document }>,
      )
      : null;
    if (!rawWin || !rawDoc) return null;
    const rawElement = unwrapElement(
      element as HTMLElement & Partial<{ wrappedJSObject: HTMLElement }>,
    );
    const InputEv = (rawWin.InputEvent ?? null) as typeof InputEvent | null;
    const EventCtor = rawWin.Event ?? globalThis.Event;
    const cloneOpts = (opts: object) =>
      this.deps.eventDispatcher.cloneIntoPageContext(opts);
    return { rawWin, rawDoc, rawElement, InputEv, EventCtor, cloneOpts };
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
      const element = this.deepQuery(selector) as
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
        const highlightOptions = this.deps.highlightManager.getHighlightOptions(
          "Input",
        );

        this.deps.highlightManager
          .applyHighlight(element, highlightOptions, elementInfo)
          .catch(() => {});
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
      const typingDelay = typeof options.typingDelayMs === "number"
        ? Math.max(0, options.typingDelayMs)
        : 25;

      const rawWin = unwrapWindow(win);
      const rawElement = unwrapElement(
        element as
          & HTMLInputElement
          & Partial<{ wrappedJSObject: HTMLInputElement }>,
      );
      if (!rawWin) return false;

      const InputEv = rawWin.InputEvent ?? null;
      const EventCtor = rawWin.Event ?? globalThis.Event;
      const KeyboardEv = rawWin.KeyboardEvent ?? globalThis.KeyboardEvent;
      const FocusEv = rawWin.FocusEvent ?? globalThis.FocusEvent;

      const cloneOpts = (opts: object) =>
        this.deps.eventDispatcher.cloneIntoPageContext(opts);

      const dispatchBeforeInput = (data: string) => {
        try {
          if (InputEv) {
            rawElement.dispatchEvent(
              new InputEv(
                "beforeinput",
                cloneOpts({
                  bubbles: true,
                  cancelable: true,
                  inputType: "insertText",
                  data,
                }),
              ),
            );
          }
        } catch {
          // ignore
        }
      };

      const setValue = setter ? (v: string) => setter(v) : (v: string) => {
        rawElement.value = v;
      };

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
        new FocusEv("blur", cloneOpts({ bubbles: false })),
      );
      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error setting input value:", e);
      return false;
    }
  }

  async clearInput(selector: string): Promise<boolean> {
    try {
      const element = this.deepQuery(selector) as
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

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

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
      const element = this.deepQuery(selector) as HTMLSelectElement | null;
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
          (opt.textContent ?? "").toLowerCase().includes(lower)
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
        const highlightOptions = this.deps.highlightManager.getHighlightOptions(
          "Input",
        );

        this.deps.highlightManager
          .applyHighlight(element, highlightOptions, elementInfo)
          .catch(() => {});
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const setter = this.deps.eventDispatcher.getNativeSelectValueSetter(
        element,
      );
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
      const element = this.deepQuery(selector) as HTMLInputElement | null;
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

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const setter = this.deps.eventDispatcher.getNativeCheckedSetter(element);
      if (setter) {
        setter(checked);
      } else {
        element.checked = checked;
      }

      // Reflect state for attribute-based checks/serializations on both wrappers
      const rawElement = unwrapElement(
        element as
          & HTMLInputElement
          & Partial<{ wrappedJSObject: HTMLInputElement }>,
      );

      const syncAttrs = (target: HTMLInputElement) => {
        try {
          if (checked) {
            target.setAttribute("checked", "true");
            target.defaultChecked = true;
            target.setAttribute("aria-checked", "true");
          } else {
            target.removeAttribute("checked");
            target.defaultChecked = false;
            target.setAttribute("aria-checked", "false");
          }
        } catch (e) {
          console.error("[NRWebScraper] syncAttrs failed:", e);
        }
      };

      syncAttrs(element);
      if (rawElement !== element) {
        syncAttrs(rawElement);
      }

      this.deps.eventDispatcher.dispatchInputEvents(element);

      if (element.type === "radio" && checked) {
        const win = this.contentWindow;
        const rawWin = unwrapWindow(win);
        const rawElement = unwrapElement(
          element as
            & HTMLInputElement
            & Partial<{ wrappedJSObject: HTMLInputElement }>,
        );
        if (rawWin) {
          const MouseEv = rawWin.MouseEvent ?? globalThis.MouseEvent;
          rawElement.dispatchEvent(
            new MouseEv(
              "click",
              this.deps.eventDispatcher.cloneIntoPageContext({ bubbles: true }),
            ),
          );
        }
      }

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error setting checked state:", e);
      return false;
    }
  }

  async uploadFile(
    selector: string,
    fileData: number[],
    fileName: string,
  ): Promise<boolean> {
    try {
      const element = this.deepQuery(selector) as HTMLInputElement | null;

      if (!element || element.tagName !== "INPUT" || element.type !== "file") {
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.deps.translationHelper.translate(
        "uploadFile",
        {
          value: this.deps.translationHelper.truncate(
            fileName,
            30,
          ),
        },
      );
      const options = this.deps.highlightManager.getHighlightOptions("Input");
      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      type MozFileInput = HTMLInputElement & {
        mozSetFileArray?: (files: File[]) => void;
      };
      const fileInput = element as MozFileInput;

      try {
        // Get the raw (unwrapped) element first
        const rawElement = unwrapElement(
          fileInput as Element & Partial<{ wrappedJSObject: Element }>,
        ) as MozFileInput;

        // The trusted parent-side service reads the selected file before the
        // message reaches this untrusted content process. Never accept a path
        // here or send one back to NRWebScraperParent.
        const fileBytes = new Uint8Array(fileData);

        // Detect MIME type from extension
        const ext = fileName.split(".").pop()?.toLowerCase() ?? "";
        const mimeTypes: Record<string, string> = {
          txt: "text/plain",
          pdf: "application/pdf",
          png: "image/png",
          jpg: "image/jpeg",
          jpeg: "image/jpeg",
          gif: "image/gif",
          webp: "image/webp",
          svg: "image/svg+xml",
          json: "application/json",
          xml: "application/xml",
          html: "text/html",
          css: "text/css",
          js: "text/javascript",
          zip: "application/zip",
          doc: "application/msword",
          docx:
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
          xls: "application/vnd.ms-excel",
          xlsx:
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        };
        const mimeType = mimeTypes[ext] ?? "application/octet-stream";

        // Get the content window for creating content-space objects
        const win = this.contentWindow;
        const rawWin = win ? unwrapWindow(win) : null;

        if (!rawWin) {
          console.error("[NRWebScraper] No content window available");
          return false;
        }

        // Clone the file data array into content window's context using Cu.cloneInto
        // This is necessary because both the Uint8Array and the array wrapper are privileged objects
        const clonedBlobParts = Cu.cloneInto([fileBytes], rawWin);
        const clonedBlobOptions = Cu.cloneInto({ type: mimeType }, rawWin);

        // Use content window's constructors to avoid security wrapper issues
        const ContentBlob = rawWin.Blob ?? Blob;
        const ContentFile = rawWin.File ?? File;

        // Create a Blob from the cloned file data using content window's Blob
        const blob = new ContentBlob(clonedBlobParts, clonedBlobOptions);

        // Clone the file parts array for the File constructor
        const clonedFileParts = Cu.cloneInto([blob], rawWin);
        const clonedFileOptions = Cu.cloneInto({ type: mimeType }, rawWin);

        // Create a File object from the Blob using content window's File
        const file = new ContentFile(
          clonedFileParts,
          fileName,
          clonedFileOptions,
        );

        // mozSetFileArray should accept File objects
        if (typeof rawElement.mozSetFileArray === "function") {
          rawElement.mozSetFileArray([file]);
        } else if (typeof fileInput.mozSetFileArray === "function") {
          fileInput.mozSetFileArray([file]);
        } else {
          console.error(
            "DOMWriteOperations: mozSetFileArray not available on either wrapped or raw element",
          );
          return false;
        }

        // Dispatch events using content window's Event constructor
        // Clone the event options to avoid security wrapper issues
        const ContentEvent = rawWin.Event ?? Event;
        const eventOptions = Cu.cloneInto({ bubbles: true }, rawWin);
        rawElement.dispatchEvent(new ContentEvent("input", eventOptions));
        rawElement.dispatchEvent(new ContentEvent("change", eventOptions));
      } catch (e) {
        console.error(
          "DOMWriteOperations: Failed to create upload file:",
          e,
        );
        return false;
      }

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error uploading file:", e);
      return false;
    }
  }

  /**
   * Dispatches a proper text input event sequence for rich text editors.
   * This fires beforeinput with inputType: insertText, which Draft.js and similar
   * frameworks listen for to update their internal state.
   *
   * Unlike setTextContent, this does NOT set textContent directly - it lets the
   * editor handle the text insertion via the beforeinput event.
   *
   * Event sequence:
   * 1. beforeinput (cancelable) - If cancelled, editor handles insertion
   * 2. input (non-cancelable) - Fired only if beforeinput was not cancelled
   * 3. change (bubbles) - Fired only if beforeinput was not cancelled
   *
   * Note: The actual text insertion is expected to be handled by the editor
   * in response to the beforeinput event. We don't set textContent directly
   * as it would break Draft.js's internal state.
   */
  dispatchTextInput(selector: string, text: string): boolean {
    try {
      const doc = this.document;
      if (!doc) return false;

      const element = deepQuerySelector(doc, selector) as HTMLElement | null;
      if (!element) {
        console.warn(
          `DOMWriteOperations: Element not found for dispatchTextInput: ${selector}`,
        );
        return false;
      }

      this.deps.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.deps.eventDispatcher.focusElementSoft(element);

      const ctx = this.getWriteContext(element);
      if (!ctx) return false;
      const { rawWin, rawDoc, rawElement, InputEv, EventCtor, cloneOpts } = ctx;

      // 1. Fire beforeinput (this is what Draft.js listens for)
      if (InputEv) {
        const beforeInputEvent = new InputEv(
          "beforeinput",
          cloneOpts({
            bubbles: true,
            cancelable: true,
            inputType: "insertText",
            data: text,
          }),
        );
        const notCancelled = rawElement.dispatchEvent(beforeInputEvent);

        // If the editor cancelled the event, it will handle the insertion itself
        // Don't set textContent - let the editor do it
        if (notCancelled) {
          // 2. Fire input event for good measure
          rawElement.dispatchEvent(
            new InputEv(
              "input",
              cloneOpts({
                bubbles: true,
                cancelable: false,
                inputType: "insertText",
                data: text,
              }),
            ),
          );

          // 3. Fire change event
          rawElement.dispatchEvent(
            new EventCtor("change", cloneOpts({ bubbles: true })),
          );
          return true;
        }

        // Editor handled it via beforeinput
        return true;
      }

      // Fallback: try execCommand (deprecated but works in many cases)
      // Note: tryExecCommand is responsible for dispatching input/change events.
      if (this.tryExecCommand(rawWin, rawDoc, rawElement, "insertText", text)) {
        return true;
      }

      // No fallback available - setting textContent directly breaks Draft.js
      // because it doesn't update the editor's internal EditorState
      console.warn(
        "DOMWriteOperations: dispatchTextInput failed - no fallback available for this editor",
      );
      return false;
    } catch (e) {
      console.error("DOMWriteOperations: Error in dispatchTextInput:", e);
      return false;
    }
  }

  setCookieString(
    cookieString: string,
    cookieName?: string,
    cookieValue?: string,
  ): boolean {
    try {
      // SECURITY WARNING: This method directly sets cookies without validation.
      // Ensure cookieString is from a trusted source to avoid cookie injection attacks.
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

      const element = deepQuerySelector(doc, selector) as HTMLElement | null;
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

      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      // SECURITY WARNING: This method directly sets innerHTML which can execute malicious scripts.
      // Only use with trusted content. For user-provided content, consider using textContent instead.
      const ctx = this.getWriteContext(element);
      if (!ctx) return false;
      const { rawWin, rawDoc, rawElement, InputEv, EventCtor, cloneOpts } = ctx;

      if (this.tryExecCommand(rawWin, rawDoc, rawElement, "insertHTML", html)) {
        return true;
      }

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv(
            "beforeinput",
            cloneOpts({
              bubbles: true,
              cancelable: true,
              inputType: "insertHTML",
              data: null,
            }),
          ),
        );
      }

      if (rawElement !== element) {
        rawElement.innerHTML = html;
      } else {
        element.innerHTML = html;
      }

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv(
            "input",
            cloneOpts({
              bubbles: true,
              cancelable: false,
              inputType: "insertHTML",
            }),
          ),
        );
      } else {
        rawElement.dispatchEvent(
          new EventCtor(
            "input",
            cloneOpts({ bubbles: true, cancelable: true }),
          ),
        );
      }

      rawElement.dispatchEvent(
        new EventCtor("change", cloneOpts({ bubbles: true })),
      );

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

      const element = deepQuerySelector(doc, selector) as HTMLElement | null;
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
      this.deps.highlightManager
        .applyHighlight(element, options, elementInfo)
        .catch(() => {});

      const ctx = this.getWriteContext(element);
      if (!ctx) return false;
      const { rawWin, rawDoc, rawElement, InputEv, EventCtor, cloneOpts } = ctx;

      // Note: tryExecCommand is responsible for dispatching input/change events.
      if (this.tryExecCommand(rawWin, rawDoc, rawElement, "insertText", text)) {
        return true;
      }

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv(
            "beforeinput",
            cloneOpts({
              bubbles: true,
              cancelable: true,
              inputType: "insertText",
              data: text,
            }),
          ),
        );
      }

      if (rawElement !== element) {
        rawElement.textContent = text;
      } else {
        element.textContent = text;
      }

      if (InputEv) {
        rawElement.dispatchEvent(
          new InputEv(
            "input",
            cloneOpts({
              bubbles: true,
              cancelable: false,
              inputType: "insertText",
              data: text,
            }),
          ),
        );
      } else {
        rawElement.dispatchEvent(
          new EventCtor(
            "input",
            cloneOpts({ bubbles: true, cancelable: true }),
          ),
        );
      }

      rawElement.dispatchEvent(
        new EventCtor("change", cloneOpts({ bubbles: true })),
      );

      return true;
    } catch (e) {
      console.error("DOMWriteOperations: Error in setTextContent:", e);
      return false;
    }
  }
}
