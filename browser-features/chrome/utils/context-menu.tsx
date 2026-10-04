import type { ComponentChild } from "preact";
import { safeRender } from "@nora/preact-xul";
import { addDisposer, createRoot } from "@nora/preact-xul/lifetime";
import { useSignal } from "@preact/signals";
import { useEffect } from "preact/hooks";
import i18next from "i18next";
import { FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE } from "#features-chrome/common/context-menu/style.ts";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";

// deno-lint-ignore no-namespace
export namespace ContextMenuUtils {
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
    const container = contentAreaContextMenu();
    const targetNode = document?.getElementById(checkID) as XULElement | null;
    const renderElement = document?.getElementById(
      renderElementId,
    ) as XULElement | null;

    if (!targetNode || !renderElement) {
      console.warn(
        "[ContextMenu]",
        `Element not found: ${!targetNode ? checkID : renderElementId}`,
      );
      return;
    }

    if (!container) return;
    return createRoot((dispose) => {
      safeRender(
        ContextMenu(id, l10n, runFunction, semanticKey),
        container as unknown as Element,
        {
          marker:
            renderElement.parentElement === (container as unknown as Element)
              ? renderElement
              : undefined,
        },
      );
      const observer = new MutationObserver(checkedFunction);
      observer.observe(targetNode, { attributes: true });
      addDisposer(() => observer.disconnect());
      checkedFunction();
      return dispose;
    });
  }

  export function addToolbarContentMenuPopupSet(
    JSXElem: () => ComponentChild,
  ) {
    if (document?.body) {
      safeRender(JSXElem, document.body);
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

// Internal preact component for reactive label via @preact/signals
function ContextMenuEl(
  { id, l10n, runFunction, semanticKey }: {
    id: string;
    l10n: string;
    runFunction: () => void;
    semanticKey?: string;
  },
) {
  const label = useSignal(i18next.t(l10n));

  useEffect(() => {
    // Register i18n observer once on mount
    return addI18nObserver(() => {
      label.value = i18next.t(l10n);
    });
  }, []);

  return (
    <xul:menuitem
      label={label.value}
      id={id}
      data-floorp-context-menu-key={semanticKey}
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
  semanticKey?: string,
): ComponentChild {
  return (
    <ContextMenuEl
      id={id}
      l10n={l10n}
      runFunction={runFunction}
      semanticKey={semanticKey}
    />
  );
}
