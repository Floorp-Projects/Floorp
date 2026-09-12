import { render } from "@nora/solid-xul";
import i18next from "i18next";
import { createSignal } from "solid-js";
import type { JSXElement } from "solid-js";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import { createRootHMR } from "@nora/solid-xul";
import { FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE } from "#features-chrome/common/context-menu/style.ts";

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

  export function onPopupShowing() {
    console.log("onpopupshowing");
    if (!screenShotContextMenuItems()?.hidden) {
      const sep = pdfjsContextMenuSeparator();
      if (sep) sep.hidden = false;

      const nextSibling = screenShotContextMenuItems()
        ?.nextSibling as XULElement;
      if (nextSibling) nextSibling.hidden = false;
    }

    (() => {
      const separators = contextMenuSeparators();
      // Undo only the state this helper applied on an earlier opening. This
      // lets the current Firefox visibility conditions be evaluated afresh
      // without touching separators hidden by Firefox itself.
      for (const contextMenuSeparator of separators) {
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

      for (const contextMenuSeparator of separators) {
        const nextSibling = contextMenuSeparator.nextSibling as XULElement;

        if (
          nextSibling?.hidden &&
          contextMenuSeparator.id !== "context-sep-navigation" &&
          contextMenuSeparator.id !== "context-sep-pdfjs-selectall"
        ) {
          if (!contextMenuSeparator.hidden) {
            contextMenuSeparator.setAttribute(
              FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE,
              "true",
            );
          }
          contextMenuSeparator.hidden = true;
        }
      }
    })();
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
