/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared Xray unwrap helpers used across webscraper modules
 */

import type { ContentWindow, RawContentWindow } from "./types.ts";

/**
 * Helper to unwrap Xray-wrapped window
 */
export function unwrapWindow(
  win: ContentWindow | null,
): RawContentWindow | null {
  if (!win) return null;
  return win.wrappedJSObject ?? (win as unknown as RawContentWindow);
}

/**
 * Helper to unwrap Xray-wrapped element
 */
export function unwrapElement<T extends Element>(
  element: T & Partial<{ wrappedJSObject: T }>,
): T {
  return element.wrappedJSObject ?? element;
}

/**
 * Helper to unwrap Xray-wrapped document
 */
export function unwrapDocument(
  doc: Document & Partial<{ wrappedJSObject: Document }>,
): Document {
  return doc.wrappedJSObject ?? doc;
}

const MAX_SHADOW_DEPTH = 5;

/**
 * querySelector that pierces open shadow DOM boundaries.
 * Tries the fast path (light DOM) first, then recursively searches shadow roots.
 */
export function deepQuerySelector(
  root: Document | Element | ShadowRoot,
  selector: string,
): Element | null {
  // Fast path: light DOM
  const result = root.querySelector(selector);
  if (result) return result;

  // Traverse into shadow roots
  return deepQuerySelectorImpl(root, selector, 0);
}

function deepQuerySelectorImpl(
  root: Document | Element | ShadowRoot,
  selector: string,
  depth: number,
): Element | null {
  if (depth >= MAX_SHADOW_DEPTH) return null;

  const walker = (root as Document).createTreeWalker?.(
    root.nodeType === 9
      ? (root as Document).body ?? (root as Document).documentElement
      : root,
    0x1, /* NodeFilter.SHOW_ELEMENT */
  ) ??
    (root as Element).ownerDocument?.createTreeWalker?.(
      root,
      0x1,
    );
  if (!walker) return null;

  let node = walker.nextNode() as Element | null;
  while (node) {
    const shadowRoot = (
      node as unknown as { shadowRoot?: ShadowRoot }
    ).shadowRoot;
    if (shadowRoot) {
      // First try direct querySelector on the shadow root
      const found = shadowRoot.querySelector(selector);
      if (found) return found;
      // Then recurse deeper
      const deep = deepQuerySelectorImpl(shadowRoot, selector, depth + 1);
      if (deep) return deep;
    }
    node = walker.nextNode() as Element | null;
  }
  return null;
}

/**
 * querySelectorAll that pierces open shadow DOM boundaries.
 * Collects all matching elements from light DOM and all shadow roots.
 */
export function deepQuerySelectorAll(
  root: Document | Element | ShadowRoot,
  selector: string,
): Element[] {
  const results = Array.from(root.querySelectorAll(selector));
  deepQuerySelectorAllImpl(root, selector, results, 0);
  return results;
}

function deepQuerySelectorAllImpl(
  root: Document | Element | ShadowRoot,
  selector: string,
  results: Element[],
  depth: number,
): void {
  if (depth >= MAX_SHADOW_DEPTH) return;

  const walker = (root as Document).createTreeWalker?.(
    root.nodeType === 9
      ? (root as Document).body ?? (root as Document).documentElement
      : root,
    0x1,
  ) ??
    (root as Element).ownerDocument?.createTreeWalker?.(
      root,
      0x1,
    );
  if (!walker) return;

  let node = walker.nextNode() as Element | null;
  while (node) {
    const shadowRoot = (
      node as unknown as { shadowRoot?: ShadowRoot }
    ).shadowRoot;
    if (shadowRoot) {
      results.push(...Array.from(shadowRoot.querySelectorAll(selector)));
      deepQuerySelectorAllImpl(shadowRoot, selector, results, depth + 1);
    }
    node = walker.nextNode() as Element | null;
  }
}

/**
 * Checks if an element is visible by examining its computed styles and dimensions.
 * Shared by DOMReadOperations.isVisible and DOMWaitOperations.isVisible
 * to avoid duplicating the same logic.
 */
export function isElementVisible(
  element: Element,
  win: Window & typeof globalThis,
): boolean {
  try {
    const style = win.getComputedStyle(element);
    const rect = element.getBoundingClientRect();
    return (
      !!style &&
      style.getPropertyValue("display") !== "none" &&
      style.getPropertyValue("visibility") !== "hidden" &&
      style.getPropertyValue("opacity") !== "0" &&
      rect.width > 0 &&
      rect.height > 0
    );
  } catch {
    return false;
  }
}
