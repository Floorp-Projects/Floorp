// SPDX-License-Identifier: MPL-2.0

export const FLOORP_CONTEXT_HIDDEN_ATTRIBUTE = "data-floorp-context-hidden";
export const FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE =
  "data-floorp-context-separator-hidden";
/** Marker owned by the legacy content-menu separator cleanup. */
export const FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE =
  "data-floorp-legacy-separator-hidden";

const STYLE_ID = "floorp-context-menu-customization-style";
const XHTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const STYLE_TEXT = `
[${FLOORP_CONTEXT_HIDDEN_ATTRIBUTE}],
[${FLOORP_CONTEXT_SEPARATOR_HIDDEN_ATTRIBUTE}] {
  display: none !important;
}
`;
const STYLE_URI = `data:text/css;charset=utf-8,${
  encodeURIComponent(STYLE_TEXT)
}`;

interface ElementStyleEntry {
  kind: "element";
  element: Element;
  references: number;
  owned: boolean;
}

interface WindowStyleEntry {
  kind: "window-sheet";
  window: Window;
  windowUtils: nsIDOMWindowUtils;
  sheetType: number;
  references: number;
  owned: boolean;
}

type StyleEntry = ElementStyleEntry | WindowStyleEntry;

const styleEntries = new WeakMap<Document, StyleEntry>();

export interface ContextMenuStyleLease {
  release(): void;
}

function acquireWindowStyle(document: Document): WindowStyleEntry | null {
  const targetWindow = document.defaultView;
  if (!targetWindow || targetWindow.closed) return null;

  let windowUtils: nsIDOMWindowUtils;
  try {
    windowUtils = targetWindow.windowUtils;
  } catch {
    return null;
  }

  const sheetType = windowUtils.AUTHOR_SHEET ?? 2;
  let owned = false;
  try {
    // Additional document sheets are namespace-independent, synchronous and
    // do not rely on inline-style CSP. This is required for Places XUL docs.
    windowUtils.loadSheetUsingURIString(STYLE_URI, sheetType);
    owned = true;
  } catch {
    // Gecko throws when the same URI is already loaded. Treat that sheet like
    // an externally owned style so a second module instance never removes it.
  }

  return {
    kind: "window-sheet",
    window: targetWindow,
    windowUtils,
    sheetType,
    references: 0,
    owned,
  };
}

function acquireElementStyle(document: Document): ElementStyleEntry {
  const existing = document.getElementById(STYLE_ID);
  if (existing) {
    return {
      kind: "element",
      element: existing,
      references: 0,
      owned: false,
    };
  }

  // Non-Gecko test documents do not expose windowUtils. Use an XHTML style
  // explicitly so this fallback also works in XML/XUL documents.
  const style = document.createElementNS(XHTML_NAMESPACE, "style");
  style.id = STYLE_ID;
  style.setAttribute("data-floorp-context-menu-style", "true");
  style.textContent = STYLE_TEXT;
  (document.querySelector("head") ?? document.documentElement).appendChild(
    style,
  );
  return {
    kind: "element",
    element: style,
    references: 0,
    owned: true,
  };
}

export function acquireContextMenuStyle(
  document: Document,
): ContextMenuStyleLease {
  let entry = styleEntries.get(document);
  if (!entry) {
    const existing = document.getElementById(STYLE_ID);
    if (existing) {
      entry = {
        kind: "element",
        element: existing,
        references: 0,
        owned: false,
      };
    } else {
      entry = acquireWindowStyle(document) ?? acquireElementStyle(document);
    }
    styleEntries.set(document, entry);
  }

  entry.references++;
  let released = false;
  return {
    release(): void {
      if (released) return;
      released = true;
      const current = styleEntries.get(document);
      if (!current) return;
      current.references = Math.max(0, current.references - 1);
      if (current.references !== 0) return;
      if (current.owned) {
        if (current.kind === "element") {
          current.element.remove();
        } else if (!current.window.closed) {
          try {
            current.windowUtils.removeSheetUsingURIString(
              STYLE_URI,
              current.sheetType,
            );
          } catch (error) {
            console.error(
              "[ContextMenuCustomizer] Failed to release document stylesheet",
              error,
            );
          }
        }
      }
      styleEntries.delete(document);
    },
  };
}
