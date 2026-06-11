/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import type { ClickableElementInfo } from "./types.ts";

const ARIA_CLICKABLE_ROLES = new Set([
  "button", "tab", "link", "checkbox", "menuitem",
  "menuitemcheckbox", "menuitemradio", "radio", "textbox",
]);

export class ElementDetector {
  /**
   * Detect all clickable elements visible in the viewport.
   * @param win - The content window to scan
   * @returns Array of clickable element info
   */
  detect(win: Window): ClickableElementInfo[] {
    const doc = win.document;
    if (!doc) return [];

    const results: ClickableElementInfo[] = [];
    const body = doc.body;
    if (!body) return [];

    // Walk all elements
    const allElements = doc.querySelectorAll("*");
    for (const el of allElements) {
      if (!(el instanceof HTMLElement)) continue;
      if (!this.isInViewport(el, win)) continue;
      if (!this.isVisible(el, win)) continue;

      const clickable = this.isClickable(el);
      if (!clickable.isClickable) continue;

      const rect = el.getBoundingClientRect();
      results.push({
        element: el,
        rect,
        href: this.getHref(el),
        text: this.getText(el),
        tagName: el.tagName.toLowerCase(),
        possibleFalsePositive: clickable.possibleFalsePositive,
      });
    }

    // Sort by position (top to bottom, left to right) for consistent hint assignment
    results.sort((a, b) => {
      const verticalDiff = a.rect.top - b.rect.top;
      if (Math.abs(verticalDiff) > 10) return verticalDiff;
      return a.rect.left - b.rect.left;
    });

    return results;
  }

  private isClickable(el: HTMLElement): { isClickable: boolean; possibleFalsePositive: boolean } {
    const tagName = el.tagName.toLowerCase();

    // Skip disabled elements
    if ((el as HTMLInputElement).disabled) {
      return { isClickable: false, possibleFalsePositive: false };
    }

    // ARIA disabled
    if (el.getAttribute("aria-disabled") === "true" || el.getAttribute("aria-disabled") === "") {
      return { isClickable: false, possibleFalsePositive: false };
    }

    // onclick attribute
    if (el.hasAttribute("onclick")) {
      return { isClickable: true, possibleFalsePositive: false };
    }

    // AngularJS directives
    if (el.hasAttribute("ng-click") || el.hasAttribute("data-ng-click") || el.hasAttribute("x-ng-click")) {
      return { isClickable: true, possibleFalsePositive: false };
    }

    // ARIA roles
    const role = el.getAttribute("role");
    if (role && ARIA_CLICKABLE_ROLES.has(role)) {
      return { isClickable: true, possibleFalsePositive: false };
    }

    // contentEditable
    const contentEditable = el.getAttribute("contenteditable");
    if (contentEditable === "" || contentEditable === "true" || contentEditable === "contenteditable") {
      return { isClickable: true, possibleFalsePositive: false };
    }

    // jsaction (Google's event system)
    const jsaction = el.getAttribute("jsaction");
    if (jsaction) {
      // Parse semicolon-separated rules, check for click events
      const rules = jsaction.split(";");
      for (const rule of rules) {
        const parts = rule.trim().split(":");
        if (parts.length >= 2) {
          const eventType = parts[0].trim();
          const namespace = parts.length > 2 ? parts[1].trim() : "";
          if (eventType === "click" && namespace !== "none") {
            return { isClickable: true, possibleFalsePositive: false };
          }
        }
      }
    }

    // Native tag checks
    switch (tagName) {
      case "a":
        return { isClickable: true, possibleFalsePositive: false };
      case "button":
        return { isClickable: true, possibleFalsePositive: false };
      case "select":
        return { isClickable: true, possibleFalsePositive: false };
      case "textarea":
        if (!(el as HTMLTextAreaElement).readOnly) {
          return { isClickable: true, possibleFalsePositive: false };
        }
        break;
      case "input": {
        const inputType = (el as HTMLInputElement).type?.toLowerCase();
        if (inputType === "hidden") break;
        if ((el as HTMLInputElement).readOnly && !["radio", "checkbox", "select"].includes(inputType)) break;
        return { isClickable: true, possibleFalsePositive: false };
      }
      case "label": {
        const forAttr = el.getAttribute("for");
        if (forAttr) {
          const target = el.ownerDocument?.getElementById(forAttr);
          if (target && !(target as HTMLInputElement).disabled) {
            return { isClickable: true, possibleFalsePositive: false };
          }
        }
        break;
      }
      case "details":
        return { isClickable: true, possibleFalsePositive: false };
      case "object":
      case "embed":
        return { isClickable: true, possibleFalsePositive: false };
      case "img": {
        const cursor = el.ownerDocument.defaultView?.getComputedStyle(el)?.cursor;
        if (cursor === "zoom-in" || cursor === "zoom-out" || cursor === "pointer") {
          return { isClickable: true, possibleFalsePositive: false };
        }
        break;
      }
    }

    // Class heuristics (button/btn in class name)
    const className = el.className;
    if (typeof className === "string" && /(?:^|\s)(?:button|btn)(?:\s|$)/i.test(className)) {
      return { isClickable: true, possibleFalsePositive: true };
    }

    // tabindex >= 0
    const tabIndex = el.getAttribute("tabindex");
    if (tabIndex !== null) {
      const idx = parseInt(tabIndex, 10);
      if (idx >= 0) {
        return { isClickable: true, possibleFalsePositive: false };
      }
    }

    return { isClickable: false, possibleFalsePositive: false };
  }

  private isVisible(el: HTMLElement, win: Window): boolean {
    const style = win.getComputedStyle(el);
    if (style.display === "none") return false;
    if (style.visibility === "hidden") return false;
    if (style.opacity === "0") return false;
    if (parseFloat(style.opacity) <= 0) return false;
    return true;
  }

  private isInViewport(el: HTMLElement, win: Window): boolean {
    const rect = el.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) return false;
    // Check if element is at least partially in viewport
    return (
      rect.bottom >= 0 &&
      rect.top <= win.innerHeight &&
      rect.right >= 0 &&
      rect.left <= win.innerWidth
    );
  }

  private getHref(el: HTMLElement): string | null {
    if (el.tagName.toLowerCase() === "a") {
      return (el as HTMLAnchorElement).href || null;
    }
    // Check for closest anchor parent
    const closestA = el.closest("a");
    if (closestA) {
      return closestA.href || null;
    }
    return null;
  }

  private getText(el: HTMLElement): string | null {
    const text = el.textContent?.trim() ?? "";
    return text.length > 0 ? text.substring(0, 200) : null;
  }
}
