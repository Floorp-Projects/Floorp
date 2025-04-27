import { render } from "@nora/solid-xul";
import type { JSXElement } from "solid-js";

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
  ) {
    const contextMenu = ContextMenu(id, l10n, runFunction);
    const targetNode = document?.getElementById(checkID) as XULElement;
    const renderElement = document?.getElementById(
      renderElementId,
    ) as XULElement;

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

  export function addToolbarContentMenuPopupSet(
    JSXElem: () => JSXElement,
  ) {
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

    (async () => {
      for (const contextMenuSeparator of contextMenuSeparators()) {
        const nextSibling = contextMenuSeparator.nextSibling as XULElement;
        
        if (
          nextSibling?.hidden &&
          contextMenuSeparator.id !== "context-sep-navigation" &&
          contextMenuSeparator.id !== "context-sep-pdfjs-selectall"
        ) {
          contextMenuSeparator.hidden = true;
        }
      }
    })();
  }
}

export function ContextMenu(id: string, l10n: string, runFunction: () => void) {
  return (
    <xul:menuitem
      data-l10n-id={l10n}
      label={l10n}
      id={id}
      onCommand={runFunction}
    />
  );
}
