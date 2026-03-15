/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";
import { unwrapElement } from "./utils.ts";
import { TurndownService } from "./turndown/index.ts";

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

  private markdownConverter = new TurndownService({
    headingStyle: "atx",
    codeBlockStyle: "fenced",
    bulletListMarker: "-",
  });

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

      const elements = Array.from(nodeList) as Element[];
      if (elements.length === 0) return [];

      // Run async inspection with found elements
      this.deps.highlightManager.runAsyncInspection(
        elements,
        "inspectGetElements",
        {
          selector,
          count: elements.length,
        },
      );

      // Extract outerHTML from each element
      return elements.map((el: Element) => String(el.outerHTML));
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

  // Deprecated alias for getElementText - kept for backward compatibility
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

        const directAttr = element.getAttribute(attributeName);
        if (directAttr !== null) return directAttr;

        const rawElement = unwrapElement(
          element as Element & Partial<{ wrappedJSObject: Element }>,
        );

        if (rawElement !== element) {
          const rawAttr = rawElement.getAttribute(attributeName);
          if (rawAttr !== null) return rawAttr;
        }

        if (attributeName === "aria-checked") {
          // Fallback to .checked property if it's an input
          try {
            if ("checked" in rawElement) {
              return (rawElement as HTMLInputElement).checked
                ? "true"
                : "false";
            }
          } catch (e) {
            console.error(
              "[NRWebScraper] Error accessing .checked property:",
              e,
            );
          }
        }

        return null;
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

  /**
   * Gets the page content as Markdown.
   * Uses Turndown library to convert HTML to Markdown format.
   * Excludes script/style/noscript elements for cleaner output.
   *
   * Also includes content from iframes and Shadow DOM for dynamic sites.
   */
  getText(): string | null {
    try {
      const doc = this.document;
      if (!doc?.body) return null;

      // Clone body to avoid modifying the original document
      const bodyClone = doc.body.cloneNode(true) as Element;

      // Remove non-content elements (scripts, styles, noscript) before conversion
      const elementsToRemove = bodyClone.querySelectorAll(
        "script, style, noscript",
      );
      for (const elem of Array.from(elementsToRemove)) {
        elem.remove();
      }

      // Convert body element to Markdown using TurndownService
      const markdown = this.markdownConverter.turndown(bodyClone);

      if (!markdown) return null;

      let result = markdown;

      // Also include text from Shadow DOM (for React/Web Components sites)
      const shadowText = this.getTextFromShadowDOM(doc.body);
      if (shadowText) {
        result += "\n\n---\n\n#### Shadow DOM Content\n\n" + shadowText.trim() + "\n\n";
      }

      // Also include text from iframes (for Gmail, email clients, etc.)
      const iframeText = this.getTextFromIframes(doc);
      if (iframeText) {
        result += "\n\n---\n\n#### iframe Content\n\n" + iframeText.trim() + "\n\n";
      }

      // Normalize whitespace
      return result
        .replace(/\n{3,}/g, "\n\n") // Limit consecutive newlines
        .trim();
    } catch (e) {
      console.error("DOMReadOperations: Error getting text:", e);
      return null;
    }
  }

  /**
   * Extracts text from Shadow DOM trees.
   * Uses TreeWalker for efficient traversal of large DOMs.
   */
  private getTextFromShadowDOM(root: Element): string {
    let text = "";

    try {
      // Use TreeWalker for efficient traversal (avoids querySelectorAll("*"))
      const ownerDocument = root.ownerDocument || document;
      const walker = ownerDocument.createTreeWalker(
        root,
        NodeFilter.SHOW_ELEMENT,
      );

      let node: Node | null;
      while ((node = walker.nextNode())) {
        const elem = node as Element;
        // shadowRoot exists on Element in Firefox's DOM but not in TypeScript's lib
        const shadowRoot = (elem as unknown as { shadowRoot?: ShadowRoot })
          .shadowRoot;
        if (shadowRoot) {
          // Extract text from shadow root using textContent
          const shadowBody = shadowRoot as unknown as { body?: HTMLElement };
          if (shadowBody.body) {
            const shadowText = shadowBody.body.textContent || "";
            if (shadowText) {
              text += shadowText + "\n";
            }
          } else {
            // Fallback: use textContent directly
            const shadowText = shadowRoot.textContent || "";
            if (shadowText) {
              text += shadowText + "\n";
            }
          }

          // Recursively handle nested shadow roots
          text += this.getTextFromShadowDOM(shadowRoot as Element);
        }
      }
    } catch (e) {
      console.error("DOMReadOperations: Error reading Shadow DOM:", e);
    }

    return text;
  }

  /**
   * Extracts text from same-origin iframes.
   * Cross-origin iframes are skipped due to security restrictions.
   * Also extracts text from Shadow DOM within iframes.
   */
  private getTextFromIframes(doc: Document): string {
    let text = "";

    try {
      const iframes = doc.querySelectorAll("iframe");

      for (const iframe of iframes) {
        try {
          const iframeDoc = iframe.contentDocument;
          if (iframeDoc?.body) {
            const iframeBody = iframeDoc.body as HTMLElement;
            let iframeText = iframeBody.textContent || "";

            // Also extract from Shadow DOM within iframe
            const shadowText = this.getTextFromShadowDOM(iframeBody);
            if (shadowText) {
              iframeText += "\n" + shadowText;
            }

            if (iframeText) {
              text += iframeText + "\n";
            }
          }
        } catch (e) {
          // Skip cross-origin iframes (security restriction)
          // console.debug("Skipping cross-origin iframe");
        }
      }
    } catch (e) {
      console.error("DOMReadOperations: Error reading iframes:", e);
    }

    return text;
  }
}
