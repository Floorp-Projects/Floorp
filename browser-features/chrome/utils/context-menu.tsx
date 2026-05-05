import { render } from "preact";
import type { ComponentChild } from "preact";
import { safeRender } from "@nora/preact-xul";
import { useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import i18next from "i18next";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

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
  ) {
    const container = contentAreaContextMenu();
    const renderElement = document?.getElementById(
      renderElementId,
    ) as XULElement | null;

    // Imperative DOM insertion: preact's render() manages an entire container's
    // content, so using it for multiple independent menu items would cause each
    // call to overwrite the previous. Instead, create XUL elements directly.
    if (container) {
      const menuitem = document!.createXULElement("menuitem") as XULElement;
      menuitem.setAttribute("id", id);
      menuitem.setAttribute("label", i18next.t(l10n));
      menuitem.addEventListener("command", runFunction);

      // Preserve insertion position (before marker) if available
      if (
        renderElement?.parentElement === (container as unknown as Element)
      ) {
        (container as unknown as Element).insertBefore(menuitem, renderElement);
      } else {
        (container as unknown as Element).appendChild(menuitem);
      }

      // Live i18n update: update attribute directly
      addI18nObserver(() => {
        menuitem.setAttribute("label", i18next.t(l10n));
      });
    }

    const targetNode = document?.getElementById(checkID) as XULElement | null;
    if (targetNode) {
      contextMenuObserver.observe(targetNode, { attributes: true });
    }
    checkItems.push(checkedFunction);
    contextMenuObserverFunc();
  }

  function contextMenuObserverFunc() {
    for (const checkItem of checkItems) {
      checkItem();
    }
  }

  export function addToolbarContentMenuPopupSet(
    JSXElem: () => ComponentChild,
  ) {
    if (document?.body) {
      safeRender(JSXElem() as import("preact").VNode, document.body);
    }
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

// Internal preact component for reactive label via @preact/signals
function ContextMenuEl(
  { id, l10n, runFunction }: {
    id: string;
    l10n: string;
    runFunction: () => void;
  },
) {
  const label = useSignal(i18next.t(l10n));

  useEffect(() => {
    // Register i18n observer once on mount
    addI18nObserver(() => {
      label.value = i18next.t(l10n);
    });
  }, []);

  return (
    <xul:menuitem
      label={label.value}
      id={id}
      onCommand={runFunction}
    />
  );
}

/**
 * Returns a preact VNode for a XUL menuitem with reactive i18n label.
 * When rendered by preact as a component, the label updates automatically
 * when the application language changes.
 */
export function ContextMenu(
  id: string,
  l10n: string,
  runFunction: () => void,
): ComponentChild {
  return <ContextMenuEl id={id} l10n={l10n} runFunction={runFunction} />;
}
