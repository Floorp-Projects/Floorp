/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";

/**
 * Read-only DOM utilities (queries, getters)
 */
export class DOMReadOperations {
  constructor(private deps: DOMOpsDeps) {}

  private get contentWindow(): (Window & typeof globalThis) | null {
    return this.deps.getContentWindow();
  }

  private get document(): Document | null {
    return this.deps.getDocument();
  }

  getHTML(): string | null {
    try {
      const win = this.contentWindow;
      const doc = win?.document ?? null;
      if (win && doc) {
        const html = doc.documentElement?.outerHTML;

        const docElement = doc.documentElement;
        if (docElement) {
          this.deps.highlightManager.runAsyncInspection(
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
      console.error("DOMReadOperations: Error getting HTML:", e);
      return null;
    }
  }

  getElement(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.deps.highlightManager.runAsyncInspection(
          element,
          "inspectGetElement",
          {
            selector,
          },
        );
        return String((element as Element).outerHTML);
      }
      return null;
    } catch (e) {
      console.error("DOMReadOperations: Error getting element:", e);
      return null;
    }
  }

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
        this.deps.highlightManager.runAsyncInspection(
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
      console.error("DOMReadOperations: Error getting elements:", e);
      return [];
    }
  }

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
          this.deps.highlightManager.runAsyncInspection(
            el,
            "inspectGetElementByText",
            {
              text: this.deps.translationHelper.truncate(textContent, 30),
            },
          );
          return String(el.outerHTML);
        }
      }
      return null;
    } catch (e) {
      console.error("DOMReadOperations: Error getting element by text:", e);
      return null;
    }
  }

  getElementText(selector: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.deps.highlightManager.runAsyncInspection(
          element,
          "inspectGetElementText",
          { selector },
        );
        return element.textContent?.trim() || null;
      }
      return null;
    } catch (e) {
      console.error("DOMReadOperations: Error getting element text:", e);
      return null;
    }
  }

  getElementTextContent(selector: string): string | null {
    return this.getElementText(selector);
  }

  async getValue(selector: string): Promise<string | null> {
    try {
      const element = this.document?.querySelector(selector) as
        | HTMLInputElement
        | HTMLTextAreaElement
        | HTMLSelectElement
        | null;
      if (!element) return null;

      const elementInfo = await this.deps.translationHelper.translate(
        "inspectGetValue",
        { selector },
      );
      await this.deps.highlightManager.applyHighlight(
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
      console.error("DOMReadOperations: Error getting value:", e);
      return null;
    }
  }

  getAttribute(selector: string, attributeName: string): string | null {
    try {
      const element = this.document?.querySelector(selector) as Element | null;
      if (element) {
        this.deps.highlightManager.runAsyncInspection(
          element,
          "inspectGetElement",
          {
            selector,
          },
        );
        return element.getAttribute(attributeName);
      }
      return null;
    } catch (e) {
      console.error("DOMReadOperations: Error getting attribute:", e);
      return null;
    }
  }

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
      console.error("DOMReadOperations: Error checking visibility:", e);
      return false;
    }
  }

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
      console.error("DOMReadOperations: Error checking enabled state:", e);
      return false;
    }
  }

  getPageTitle(): string | null {
    try {
      return this.document?.title ?? null;
    } catch (e) {
      console.error("DOMReadOperations: Error getting page title:", e);
      return null;
    }
  }
}
