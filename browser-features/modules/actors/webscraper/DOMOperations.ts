/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DOMOperations - DOM manipulation and element interaction operations
 */

import type {
  InputElementOptions,
  SelectOptionOptions,
  WebScraperContext,
} from "./types.ts";
import { HighlightManager } from "./HighlightManager.ts";
import { EventDispatcher } from "./EventDispatcher.ts";
import { TranslationHelper } from "./TranslationHelper.ts";

const { setTimeout: timerSetTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

/**
 * Helper class for DOM operations
 */
export class DOMOperations {
  private highlightManager: HighlightManager;
  private eventDispatcher: EventDispatcher;
  private translationHelper: TranslationHelper;

  constructor(private context: WebScraperContext) {
    this.highlightManager = new HighlightManager(context);
    this.eventDispatcher = new EventDispatcher(context);
    this.translationHelper = new TranslationHelper(context);
  }

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  get document(): Document | null {
    return this.context.document;
  }

  /**
   * Get the highlight manager instance
   */
  getHighlightManager(): HighlightManager {
    return this.highlightManager;
  }

  /**
   * Get the event dispatcher instance
   */
  getEventDispatcher(): EventDispatcher {
    return this.eventDispatcher;
  }

  /**
   * Get the translation helper instance
   */
  getTranslationHelper(): TranslationHelper {
    return this.translationHelper;
  }

  /**
   * Extracts the HTML content from the current page
   */
  getHTML(): string | null {
    try {
      if (this.contentWindow && this.contentWindow.document) {
        const html = this.contentWindow.document?.documentElement?.outerHTML;

        const docElement = this.contentWindow.document?.documentElement;
        if (docElement) {
          this.highlightManager.runAsyncInspection(
            docElement,
            "inspectPageHtml",
          );
        }

        if (!html) {
          return null;
        }
        return html.toString();
      }

      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting HTML:", e);
      return null;
    }
  }

  /**
   * Gets a specific element by CSS selector
   */
  getElement(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.highlightManager.runAsyncInspection(element, "inspectGetElement", {
          selector,
        });
        return String((element as Element).outerHTML);
      }
      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting element:", e);
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
        this.highlightManager.runAsyncInspection(
          elements,
          "inspectGetElements",
          {
            selector,
            count: elements.length,
          },
        );
      }
      return results;
    } catch (e) {
      console.error("DOMOperations: Error getting elements:", e);
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
          this.highlightManager.runAsyncInspection(
            el,
            "inspectGetElementByText",
            {
              text: this.translationHelper.truncate(textContent, 30),
            },
          );
          return String(el.outerHTML);
        }
      }
      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting element by text:", e);
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
        this.highlightManager.runAsyncInspection(
          element,
          "inspectGetElementText",
          { selector },
        );
        return element.textContent?.trim() || null;
      }
      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting element text content:", e);
      return null;
    }
  }

  /**
   * Gets the text content of an element by CSS selector
   */
  getElementText(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.highlightManager.runAsyncInspection(
          element,
          "inspectGetElementText",
          { selector },
        );
        return element.textContent?.trim() || null;
      }
      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting element text:", e);
      return null;
    }
  }

  /**
   * Gets the value of an input/textarea/select element
   */
  async getValue(selector: string): Promise<string | null> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | HTMLSelectElement
        | null;
      if (!element) return null;

      const elementInfo = await this.translationHelper.translate(
        "inspectGetValue",
        { selector },
      );
      await this.highlightManager.applyHighlight(
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

      const maybeVal = (element as { value?: unknown }).value;
      if (maybeVal !== undefined) {
        return typeof maybeVal === "string" ? maybeVal : String(maybeVal ?? "");
      }

      return element.textContent ?? "";
    } catch (e) {
      console.error("DOMOperations: Error getting value:", e);
      return null;
    }
  }

  /**
   * Gets the value of a specific attribute from an element
   */
  getAttribute(selector: string, attributeName: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.highlightManager.runAsyncInspection(element, "inspectGetElement", {
          selector,
        });
        return element.getAttribute(attributeName);
      }
      return null;
    } catch (e) {
      console.error("DOMOperations: Error getting attribute:", e);
      return null;
    }
  }

  /**
   * Checks if an element is visible in the viewport
   */
  isVisible(selector: string): boolean {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const rect = element.getBoundingClientRect();
      if (rect.width === 0 || rect.height === 0) return false;

      const style = this.contentWindow?.getComputedStyle(element);
      if (!style) return false;

      if (style.getPropertyValue("display") === "none") return false;
      if (style.getPropertyValue("visibility") === "hidden") return false;
      if (style.getPropertyValue("opacity") === "0") return false;

      return true;
    } catch (e) {
      console.error("DOMOperations: Error checking visibility:", e);
      return false;
    }
  }

  /**
   * Checks if an element is enabled (not disabled)
   */
  isEnabled(selector: string): boolean {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      if ("disabled" in element) {
        return !(element as HTMLInputElement | HTMLButtonElement).disabled;
      }

      const ariaDisabled = element.getAttribute("aria-disabled");
      if (ariaDisabled === "true") return false;

      return true;
    } catch (e) {
      console.error("DOMOperations: Error checking enabled state:", e);
      return false;
    }
  }

  /**
   * Sets the value of an input element or textarea
   */
  async inputElement(
    selector: string,
    value: string,
    options: InputElementOptions = {},
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

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      if (!options.skipHighlight) {
        const truncatedValue = this.translationHelper.truncate(value, 50);
        const elementInfo = await this.translationHelper.translate(
          "inputValueSet",
          {
            value: truncatedValue,
          },
        );
        const highlightOptions =
          this.highlightManager.getHighlightOptions("Input");

        await this.highlightManager.applyHighlight(
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

      const setter = this.eventDispatcher.getNativeValueSetter(
        element as HTMLInputElement | HTMLTextAreaElement,
      );

      const typingMode = options.typingMode === true;
      const typingDelay =
        typeof options.typingDelayMs === "number"
          ? Math.max(0, options.typingDelayMs)
          : 25;

      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
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

      const setValue = setter
        ? (v: string) => setter(v)
        : (v: string) => {
            rawElement.value = v;
          };

      const cloneOpts = (opts: object) =>
        this.eventDispatcher.cloneIntoPageContext(opts);

      if (typingMode) {
        setValue("");
        for (const ch of value.split("")) {
          dispatchBeforeInput(ch);
          setValue(rawElement.value + ch);
          rawElement.dispatchEvent(
            new EventCtor("input", cloneOpts({ bubbles: true })),
          );
          rawElement.dispatchEvent(
            new KeyboardEv("keydown", cloneOpts({ key: ch, bubbles: true })),
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
      console.error("DOMOperations: Error setting input value:", e);
      return false;
    }
  }

  /**
   * Clicks on an element by CSS selector
   */
  async clickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return Promise.resolve(false);

      const elementTagName = element.tagName?.toLowerCase() || "element";
      const elementTextRaw = element.textContent?.trim() || "";
      const truncatedText = this.translationHelper.truncate(elementTextRaw, 30);
      const elementInfo =
        elementTextRaw.length > 0
          ? await this.translationHelper.translate("clickElementWithText", {
              tag: elementTagName,
              text: truncatedText,
            })
          : await this.translationHelper.translate("clickElementNoText", {
              tag: elementTagName,
            });

      try {
        void element.nodeType;
      } catch {
        return false;
      }

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const options = this.highlightManager.getHighlightOptions("Click");

      const effectPromise = this.highlightManager
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
          console.warn("DOMOperations: input/change dispatch failed", err);
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

      let clickDispatched = this.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );

      try {
        const rawElement = (element as any).wrappedJSObject ?? element;
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
          console.warn("DOMOperations: synthetic click failed", err);
        }
      }

      if ((isButton || isLink) && !clickDispatched) {
        try {
          element.dispatchEvent(
            new Ev("submit", { bubbles: true, cancelable: true }),
          );
        } catch (err) {
          console.warn("DOMOperations: submit dispatch failed", err);
        }
      }

      const success = stateChanged || clickDispatched;

      await Promise.race([
        effectPromise,
        new Promise((resolve) => timerSetTimeout(resolve, 300)),
      ]);

      return success;
    } catch (e) {
      console.error("DOMOperations: Error clicking element:", e);
      return Promise.resolve(false);
    }
  }

  /**
   * Waits for an element to appear in the DOM
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
        await new Promise((resolve) => timerSetTimeout(resolve, 100));
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
        console.error("DOMOperations: Error waiting for element:", e);
        return false;
      }
    }

    return false;
  }

  /**
   * Waits until the document is minimally ready for scraping
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
   * Clears the value of an input or textarea element
   */
  async clearInput(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | null;
      if (!element) return false;

      if (!("value" in element)) return false;

      const elementInfo = await this.translationHelper.translate(
        "inputValueSet",
        {
          value: "(cleared)",
        },
      );
      const options = this.highlightManager.getHighlightOptions("Input");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      if (!win) return false;

      const setter = this.eventDispatcher.getNativeValueSetter(element);
      if (setter) {
        setter("");
      } else {
        element.value = "";
      }

      this.eventDispatcher.dispatchInputEvents(element);

      return true;
    } catch (e) {
      console.error("DOMOperations: Error clearing input:", e);
      return false;
    }
  }

  /**
   * Selects an option in a select element
   */
  async selectOption(
    selector: string,
    value: string,
    opts: SelectOptionOptions = {},
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
        const elementInfo = await this.translationHelper.translate(
          "selectOption",
          {
            value: targetOpt.value,
          },
        );
        const highlightOptions =
          this.highlightManager.getHighlightOptions("Input");

        await this.highlightManager.applyHighlight(
          element,
          highlightOptions,
          elementInfo,
        );
      }

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const setter = this.eventDispatcher.getNativeSelectValueSetter(element);
      if (setter) {
        setter(targetOpt.value);
      } else {
        element.value = targetOpt.value;
      }

      this.eventDispatcher.dispatchInputEvents(element);

      return true;
    } catch (e) {
      console.error("DOMOperations: Error selecting option:", e);
      return false;
    }
  }

  /**
   * Sets the checked state of a checkbox or radio button
   */
  async setChecked(selector: string, checked: boolean): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element) return false;

      if (element.tagName !== "INPUT") return false;
      if (element.type !== "checkbox" && element.type !== "radio") return false;

      const elementInfo = await this.translationHelper.translate("setChecked", {
        state: checked ? "checked" : "unchecked",
      });
      const options = this.highlightManager.getHighlightOptions("Click");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const setter = this.eventDispatcher.getNativeCheckedSetter(element);
      if (setter) {
        setter(checked);
      } else {
        element.checked = checked;
      }

      this.eventDispatcher.dispatchInputEvents(element);

      if (element.type === "radio" && checked) {
        const win = this.contentWindow;
        const rawWin = (win as any)?.wrappedJSObject ?? win;
        const rawElement = (element as any)?.wrappedJSObject ?? element;
        const MouseEv = (rawWin?.MouseEvent ??
          globalThis.MouseEvent) as typeof MouseEvent;
        rawElement.dispatchEvent(new MouseEv("click", { bubbles: true }));
      }

      return true;
    } catch (e) {
      console.error("DOMOperations: Error setting checked state:", e);
      return false;
    }
  }

  /**
   * Simulates a mouse hover over an element
   */
  async hoverElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translationHelper.translate(
        "hoverElement",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Inspect");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const MouseEv = (rawWin?.MouseEvent ??
        globalThis.MouseEvent) as typeof MouseEvent;

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
      console.error("DOMOperations: Error hovering element:", e);
      return false;
    }
  }

  /**
   * Scrolls the page to make an element visible
   */
  async scrollToElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translationHelper.translate(
        "scrollToElement",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Inspect");

      element.scrollIntoView({ behavior: "smooth", block: "center" });

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      return true;
    } catch (e) {
      console.error("DOMOperations: Error scrolling to element:", e);
      return false;
    }
  }

  /**
   * Performs a double click on an element
   */
  async doubleClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translationHelper.translate(
        "doubleClickElement",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Click");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      this.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );
      this.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        0,
      );

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
      console.error("DOMOperations: Error double clicking element:", e);
      return false;
    }
  }

  /**
   * Performs a right click (context menu) on an element
   */
  async rightClickElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLElement | null;
      if (!element) return false;

      const elementInfo = await this.translationHelper.translate(
        "rightClickElement",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Click");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const rect = element.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      this.eventDispatcher.dispatchPointerClickSequence(
        element,
        centerX,
        centerY,
        2,
      );

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
      console.error("DOMOperations: Error right clicking element:", e);
      return false;
    }
  }

  /**
   * Focuses on an element
   */
  async focusElement(selector: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(selector) as HTMLElement;
      if (!element) return false;

      this.eventDispatcher.scrollIntoViewIfNeeded(element);

      const elementInfo = await this.translationHelper.translate(
        "focusElement",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Input");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const FocusEv = (rawWin?.FocusEvent ??
        globalThis.FocusEvent) as typeof FocusEvent;

      if (typeof rawElement.focus === "function") {
        rawElement.focus();
      } else {
        element.focus();
      }

      const cloneOpts = (opts: object) =>
        this.eventDispatcher.cloneIntoPageContext(opts);

      rawElement.dispatchEvent(
        new FocusEv("focus", cloneOpts({ bubbles: false })),
      );
      rawElement.dispatchEvent(
        new FocusEv("focusin", cloneOpts({ bubbles: true })),
      );

      return true;
    } catch (e) {
      console.error("DOMOperations: Error focusing element:", e);
      return false;
    }
  }

  /**
   * Gets the page title
   */
  getPageTitle(): string | null {
    try {
      return this.document?.title ?? null;
    } catch (e) {
      console.error("DOMOperations: Error getting page title:", e);
      return null;
    }
  }

  /**
   * Press a key or key combination
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
      console.error("DOMOperations: Error pressing key:", e);
      return false;
    }
  }

  /**
   * Uploads a file through input[type=file]
   */
  async uploadFile(selector: string, filePath: string): Promise<boolean> {
    try {
      const element = this.document?.querySelector(
        selector,
      ) as HTMLInputElement | null;
      if (!element || element.tagName !== "INPUT" || element.type !== "file") {
        return false;
      }

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.translationHelper.translate("uploadFile", {
        value: this.translationHelper.truncate(
          filePath.split(/[\\/]/).pop() ?? filePath,
          30,
        ),
      });
      const options = this.highlightManager.getHighlightOptions("Input");
      await this.highlightManager.applyHighlight(element, options, elementInfo);

      type MozFileInput = HTMLInputElement & {
        mozSetFileArray?: (files: File[]) => void;
      };
      const fileInput = element as MozFileInput;

      try {
        const file = await File.createFromFileName(filePath);
        if (typeof fileInput.mozSetFileArray === "function") {
          fileInput.mozSetFileArray([file]);
        } else {
          console.error("DOMOperations: mozSetFileArray not available");
          return false;
        }
      } catch (e) {
        console.error("DOMOperations: Failed to create file from path:", e);
        return false;
      }

      element.dispatchEvent(new Event("input", { bubbles: true }));
      element.dispatchEvent(new Event("change", { bubbles: true }));
      return true;
    } catch (e) {
      console.error("DOMOperations: Error uploading file:", e);
      return false;
    }
  }

  /**
   * Sets a cookie using document.cookie
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

      this.contentWindow.document.cookie = cookieString;

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
   * Performs a drag and drop operation between two elements
   */
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

      this.eventDispatcher.scrollIntoViewIfNeeded(source);
      this.eventDispatcher.scrollIntoViewIfNeeded(target);

      const elementInfo = await this.translationHelper.translate(
        "dragAndDrop",
        {},
      );
      const options = this.highlightManager.getHighlightOptions("Input");

      await this.highlightManager.applyHighlight(source, options, elementInfo);
      await this.highlightManager.applyHighlight(target, options, elementInfo);

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
          clientX: targetX,
          clientY: targetY,
          dataTransfer,
          view: rawWin as Window & typeof globalThis,
        }),
      );

      return true;
    } catch (e) {
      console.error("DOMOperations: Error in drag and drop:", e);
      return false;
    }
  }

  /**
   * Sets the innerHTML of an element
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
          `DOMOperations: Element not found for setInnerHTML: ${selector}`,
        );
        return false;
      }

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.translationHelper.translate(
        "inputValueSet",
        {
          value: this.translationHelper.truncate(html, 30),
        },
      );
      const options = this.highlightManager.getHighlightOptions("Input");

      await this.highlightManager.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawDoc = (this.document as any)?.wrappedJSObject ?? this.document;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      try {
        if (rawWin && rawDoc.hasFocus()) {
          const selection = rawWin.getSelection();
          if (selection) {
            const range = rawDoc.createRange();
            range.selectNodeContents(rawElement);
            selection.removeAllRanges();
            selection.addRange(range);

            if (rawDoc.execCommand("insertHTML", false, html)) {
              rawElement.dispatchEvent(
                new EventCtor("input", { bubbles: true, cancelable: true }),
              );
              rawElement.dispatchEvent(
                new EventCtor("change", { bubbles: true }),
              );
              return true;
            }
          }
        }
      } catch (e) {
        console.warn("DOMOperations: execCommand failed, falling back:", e);
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
      console.error("DOMOperations: Error in setInnerHTML:", e);
      return false;
    }
  }

  /**
   * Sets the textContent of an element
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
          `DOMOperations: Element not found for setTextContent: ${selector}`,
        );
        return false;
      }

      this.eventDispatcher.scrollIntoViewIfNeeded(element);
      this.eventDispatcher.focusElementSoft(element);

      const elementInfo = await this.translationHelper.translate(
        "inputValueSet",
        {
          value: this.translationHelper.truncate(text, 30),
        },
      );
      const options = this.highlightManager.getHighlightOptions("Input");
      await this.highlightManager.applyHighlight(element, options, elementInfo);

      const win = this.contentWindow;
      const rawWin = (win as any)?.wrappedJSObject ?? win;
      const rawDoc = (this.document as any)?.wrappedJSObject ?? this.document;
      const rawElement = (element as any)?.wrappedJSObject ?? element;
      const EventCtor = (rawWin?.Event ?? globalThis.Event) as typeof Event;

      try {
        if (rawWin && rawDoc.hasFocus()) {
          const selection = rawWin.getSelection();
          if (selection) {
            const range = rawDoc.createRange();
            range.selectNodeContents(rawElement);
            selection.removeAllRanges();
            selection.addRange(range);

            if (rawDoc.execCommand("insertText", false, text)) {
              rawElement.dispatchEvent(
                new EventCtor("input", { bubbles: true, cancelable: true }),
              );
              rawElement.dispatchEvent(
                new EventCtor("change", { bubbles: true }),
              );
              return true;
            }
          }
        }
      } catch (e) {
        console.warn("DOMOperations: execCommand failed, falling back:", e);
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
      console.error("DOMOperations: Error in setTextContent:", e);
      return false;
    }
  }

  /**
   * Dispatches a custom event on an element
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
          `DOMOperations: Element not found for dispatchEvent: ${selector}`,
        );
        return false;
      }

      return this.eventDispatcher.dispatchCustomEvent(
        element,
        eventType,
        options,
      );
    } catch (e) {
      console.error("DOMOperations: Error in dispatchEvent:", e);
      return false;
    }
  }

  /**
   * Cleanup resources
   */
  destroy(): void {
    this.highlightManager.destroy();
  }
}
