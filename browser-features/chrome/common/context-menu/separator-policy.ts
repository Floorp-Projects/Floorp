// SPDX-License-Identifier: MPL-2.0

import { FLOORP_CONTEXT_HIDDEN_ATTRIBUTE } from "./style.ts";

interface ElementWithHidden extends Element {
  hidden?: boolean;
  collapsed?: boolean;
}

export function isNativelyHidden(element: Element): boolean {
  const xulElement = element as ElementWithHidden;
  const hidden = typeof xulElement.hidden === "boolean"
    ? xulElement.hidden
    : element.hasAttribute("hidden") &&
      element.getAttribute("hidden") !== "false";
  const collapsed = typeof xulElement.collapsed === "boolean"
    ? xulElement.collapsed
    : element.hasAttribute("collapsed") &&
      element.getAttribute("collapsed") !== "false";
  return hidden || collapsed;
}

function isEffectivelyHidden(element: Element): boolean {
  return isNativelyHidden(element) ||
    element.hasAttribute(FLOORP_CONTEXT_HIDDEN_ATTRIBUTE);
}

/**
 * Returns existing separator nodes that need a transient overlay. It never
 * changes Firefox's `hidden` state or creates/removes separators.
 */
export function findSeparatorsToHide(popup: Element): Element[] {
  const separatorsToHide = new Set<Element>();
  let hasVisibleItemBeforeSeparator = false;
  let lastVisibleSeparator: Element | null = null;

  for (const child of Array.from(popup.children)) {
    if (isEffectivelyHidden(child)) continue;

    if (child.localName === "menuseparator") {
      if (!hasVisibleItemBeforeSeparator) {
        separatorsToHide.add(child);
      } else {
        lastVisibleSeparator = child;
        hasVisibleItemBeforeSeparator = false;
      }
      continue;
    }

    hasVisibleItemBeforeSeparator = true;
    lastVisibleSeparator = null;
  }

  if (!hasVisibleItemBeforeSeparator && lastVisibleSeparator) {
    separatorsToHide.add(lastVisibleSeparator);
  }
  return [...separatorsToHide];
}
