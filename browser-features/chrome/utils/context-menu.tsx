import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { createSignal } from "solid-js";
import type { JSXElement } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { createRootHMR } from "@nora/solid-xul";
import { FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE } from "#features-chrome/common/context-menu/style.ts";
import { findSeparatorsToHide } from "#features-chrome/common/context-menu/separator-policy.ts";

// deno-lint-ignore no-namespace
export namespace ContextMenuUtils {
  const checkItems: (() => void)[] = [];
  const contextMenuObserver: MutationObserver = new MutationObserver(() => {
    contextMenuObserverFunc();
  });

  function windowModalDialogElem(): XULElement | null {
    return document?.querySelector("#window-modal-dialog") as XULElement | null;
  }
  function screenShotContextMenuItems(): XULElement | null {
    return document?.querySelector(
      "#context-take-screenshot",
    ) as XULElement | null;
  }
  export function contentAreaContextMenu(): XULElement | null {
    return document?.querySelector(
      "#contentAreaContextMenu",
    ) as XULElement | null;
  }
  function pdfjsContextMenuSeparator(): XULElement | null {
    return document?.querySelector(
      "#context-sep-pdfjs-selectall",
    ) as XULElement | null;
  }
  function contextMenuSeparators(): NodeListOf<XULElement> {
    return document?.querySelectorAll(
      "#contentAreaContextMenu > menuseparator",
    ) as NodeListOf<XULElement>;
  }

  export function addContextBox(
    id: string,
    l10n: string,
    renderElementId: string,
    runFunction: () => void,
    checkID: string,
    checkedFunction: () => void,
    semanticKey?: string,
  ) {
    const contextMenu = ContextMenu(id, l10n, runFunction, semanticKey);
    const targetNode = document?.getElementById(checkID) as unknown as
      | XULElement
      | null;
    const renderElement = document?.getElementById(
      renderElementId,
    ) as unknown as XULElement | null;

    if (!targetNode || !renderElement) {
      console.warn(
        "[ContextMenu]",
        `Element not found: ${!targetNode ? checkID : renderElementId}`,
      );
      return;
    }

    render(() => contextMenu, contentAreaContextMenu(), {
      marker: renderElement,
    });
    contextMenuObserver.observe(targetNode, { attributes: true });
    checkItems.push(checkedFunction);
    contextMenuObserverFunc();
  }

  function contextMenuObserverFunc() {
    for (const checkItem of checkItems) {
      checkItem();
    }
  }

  export function addToolbarContentMenuPopupSet(JSXElem: () => JSXElement) {
    render(JSXElem, document?.body, {
      marker: windowModalDialogElem() ?? undefined,
    });
  }

  /**
   * Native repair for `#contentAreaContextMenu`, run after Firefox's own
   * popupshowing builder.
   *
   * Only the menu's own popupshowing is handled. A submenu's popupshowing
   * bubbles through the parent popup; by then extensions (`menus.refresh()`)
   * or Firefox may have recomputed the parent's separators, and redoing the
   * repair against that state hid separators the user was already looking at
   * (KAN-32 / #2469).
   */
  export function onPopupShowing(event?: Event) {
    if (event && event.target !== event.currentTarget) return;

    if (!screenShotContextMenuItems()?.hidden) {
      const sep = pdfjsContextMenuSeparator();
      if (sep) sep.hidden = false;

      const nextSibling = screenShotContextMenuItems()
        ?.nextSibling as XULElement;
      if (nextSibling) nextSibling.hidden = false;
    }

    const popup = contentAreaContextMenu();
    if (!popup) return;

    // Rows added with addContextBox mirror a Firefox row through a
    // MutationObserver. The content context menu is opened from script, so
    // those records are only delivered after every popupshowing listener has
    // run. Apply them now so the separator pass sees the rows of this opening.
    contextMenuObserverFunc();
    normalizeSeparators(popup);

    // Other Floorp features toggle their rows in their own popupshowing
    // listeners, which can run after this one. Normalize again once the
    // current script has finished; this still happens before the popup paints.
    queueMicrotask(() => {
      if (popup.isConnected) normalizeSeparators(popup);
    });
  }

  function normalizeSeparators(popup: XULElement) {
    // Undo only the state this helper applied earlier. This lets the current
    // Firefox visibility conditions be evaluated afresh without touching
    // separators hidden by Firefox itself.
    for (const contextMenuSeparator of contextMenuSeparators()) {
      if (
        contextMenuSeparator.hasAttribute(
          FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE,
        )
      ) {
        contextMenuSeparator.hidden = false;
        contextMenuSeparator.removeAttribute(
          FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE,
        );
      }
    }

    // Hide only leading, duplicate and trailing separators, skipping hidden
    // rows the same way Firefox's nsContextMenu.showHideSeparators does. A
    // separator followed by hidden rows and then a visible row (for example
    // `context-sep-sendlinktodevice` before the hidden media rows and
    // "Search ...") is a real group boundary and stays visible.
    for (const separator of findSeparatorsToHide(popup)) {
      if (
        separator.id === "context-sep-navigation" ||
        separator.id === "context-sep-pdfjs-selectall"
      ) {
        continue;
      }
      separator.setAttribute(FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE, "true");
      (separator as XULElement).hidden = true;
    }
  }
}

export function ContextMenu(
  id: string,
  l10n: string,
  runFunction: () => void,
  semanticKey?: string,
) {
  const [label, setLabel] = createSignal(i18next.t(l10n));

  createRootHMR(() => {
    addI18nObserver(() => {
      setLabel(i18next.t(l10n));
    });
  }, import.meta.hot);
  return (
    <xul:menuitem
      label={label()}
      id={id}
      data-floorp-context-menu-key={semanticKey}
      onCommand={runFunction}
    />
  );
}
