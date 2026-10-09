// SPDX-License-Identifier: MPL-2.0

import { canSwapLanguages } from "./swap.ts";
import styles from "./styles.css?inline";
import type {
  LanguageSelection,
  NativeMenuList,
  TranslateSwapControllerOptions,
  TranslateSwapDocument,
} from "./types.ts";

export const BUTTON_ID = "floorp-select-translations-swap-button";
export const BUTTON_CLASS = "floorp-translate-swap-button";
export const STYLE_MARKER_ATTR = "data-floorp-translate-swap-style";

const PANEL_ID = "select-translations-panel";
const ROW_ID = "select-translations-panel-lang-selection";
const FROM_ID = "select-translations-panel-from";
const TO_ID = "select-translations-panel-to";
const FROM_POPUP_ID = "select-translations-panel-from-menupopup";
const TO_POPUP_ID = "select-translations-panel-to-menupopup";
const CONTROLLER_SLOT = "__floorpTranslateSwapController__" as const;

type ControllerHost = Record<string, unknown> & {
  [CONTROLLER_SLOT]?: TranslateSwapController;
};

function optionValues(popup: Element | null): string[] {
  if (!popup) {
    return [];
  }
  return Array.from(popup.querySelectorAll("menuitem[value]"))
    .map((item) => item.getAttribute("value") ?? "")
    .filter((value) => value !== "");
}

export class TranslateSwapController {
  private readonly doc: TranslateSwapDocument;
  private label: string;
  private started = false;

  constructor(private readonly options: TranslateSwapControllerOptions) {
    this.doc = options.document;
    this.label = options.label;
  }

  start(): void {
    if (this.started) {
      return;
    }
    this.started = true;
    this.installStyle();
    // Capture phase: the panel is a lazily materialised template, so the
    // button is injected from its first popupshowing onwards.
    this.doc.addEventListener("popupshowing", this.onPopupEvent, true);
    this.doc.addEventListener("popupshown", this.onPopupEvent, true);
    this.doc.addEventListener("command", this.onCommand, true);
    // The panel may already exist when this module is hot-reloaded.
    this.inject();
  }

  destroy(): void {
    if (!this.started) {
      return;
    }
    this.started = false;
    this.doc.removeEventListener("popupshowing", this.onPopupEvent, true);
    this.doc.removeEventListener("popupshown", this.onPopupEvent, true);
    this.doc.removeEventListener("command", this.onCommand, true);
    this.doc.getElementById(BUTTON_ID)?.remove();
    for (const style of this.doc.querySelectorAll(`[${STYLE_MARKER_ATTR}]`)) {
      style.remove();
    }
  }

  setLabel(label: string): void {
    this.label = label;
    const button = this.doc.getElementById(BUTTON_ID);
    if (button) {
      this.applyLabel(button);
    }
  }

  /** Swaps the menus and asks the native panel to translate again. */
  swap(): void {
    const selection = this.readSelection();
    if (!selection || !canSwapLanguages(selection)) {
      this.refresh();
      return;
    }
    const fromMenu = this.menuList(FROM_ID);
    const toMenu = this.menuList(TO_ID);
    if (!fromMenu || !toMenu) {
      return;
    }

    fromMenu.value = selection.to;
    toMenu.value = selection.from;

    const api = this.options.getPanelApi();
    // Programmatic value changes do not fire "command", which is what the
    // panel uses to reset its word count and refresh its dependent buttons.
    api?.onChangeFromLanguage();
    api?.onChangeToLanguage();
    // The panel starts translating from private listeners on the menulists;
    // a non-bubbling Enter keypress reaches only the target's own listener.
    toMenu.dispatchEvent(
      new KeyboardEvent("keypress", { key: "Enter", bubbles: false }),
    );

    const phase = api?.phase();
    if (phase !== "translatable" && phase !== "translating") {
      console.error(
        "[translate-swap]",
        `Panel did not start translating after swap (phase: ${phase}).`,
      );
    }
    this.refresh();
  }

  private readonly onPopupEvent = (event: Event): void => {
    const target = event.target;
    if (target instanceof Element && target.id === PANEL_ID) {
      this.inject();
      this.refresh();
    }
  };

  private readonly onCommand = (event: Event): void => {
    const target = event.target;
    const panel = this.doc.getElementById(PANEL_ID);
    if (target instanceof Element && panel?.contains(target)) {
      this.refresh();
    }
  };

  private installStyle(): void {
    if (this.doc.querySelector(`[${STYLE_MARKER_ATTR}]`)) {
      return;
    }
    const style = this.doc.createElement("style");
    style.setAttribute(STYLE_MARKER_ATTR, "true");
    style.textContent = styles;
    this.doc.head?.appendChild(style);
  }

  private inject(): void {
    if (this.doc.getElementById(BUTTON_ID)) {
      return;
    }
    const row = this.doc.getElementById(ROW_ID);
    const toColumn = this.doc.getElementById(TO_ID)?.parentElement;
    if (!row || !toColumn || toColumn.parentElement !== row) {
      return;
    }

    const button = this.doc.createXULElement("toolbarbutton");
    button.id = BUTTON_ID;
    button.className = BUTTON_CLASS;
    button.setAttribute("closemenu", "none");
    button.setAttribute("tabindex", "0");
    this.applyLabel(button);
    button.addEventListener("command", () => this.swap());
    row.insertBefore(button, toColumn);
  }

  private applyLabel(button: Element): void {
    button.setAttribute("tooltiptext", this.label);
    button.setAttribute("aria-label", this.label);
  }

  private menuList(id: string): NativeMenuList | null {
    return this.doc.getElementById(id) as NativeMenuList | null;
  }

  private readSelection(): LanguageSelection | null {
    const fromMenu = this.menuList(FROM_ID);
    const toMenu = this.menuList(TO_ID);
    if (!fromMenu || !toMenu) {
      return null;
    }
    return {
      from: fromMenu.value ?? "",
      to: toMenu.value ?? "",
      fromOptions: optionValues(this.doc.getElementById(FROM_POPUP_ID)),
      toOptions: optionValues(this.doc.getElementById(TO_POPUP_ID)),
    };
  }

  private refresh(): void {
    const button = this.doc.getElementById(BUTTON_ID);
    if (!button) {
      return;
    }
    const selection = this.readSelection();
    const swappable = selection !== null && canSwapLanguages(selection);
    button.toggleAttribute("disabled", !swappable);
  }
}

export function installTranslateSwapController(
  options: TranslateSwapControllerOptions,
  host: object = globalThis,
): TranslateSwapController {
  const controllerHost = host as ControllerHost;
  controllerHost[CONTROLLER_SLOT]?.destroy();
  const controller = new TranslateSwapController(options);
  controllerHost[CONTROLLER_SLOT] = controller;
  controller.start();
  return controller;
}

export function uninstallTranslateSwapController(
  controller: TranslateSwapController,
  host: object = globalThis,
): void {
  const controllerHost = host as ControllerHost;
  if (controllerHost[CONTROLLER_SLOT] === controller) {
    delete controllerHost[CONTROLLER_SLOT];
  }
  controller.destroy();
}
