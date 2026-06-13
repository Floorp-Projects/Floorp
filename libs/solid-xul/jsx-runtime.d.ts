// SPDX-License-Identifier: MPL-2.0

import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    interface StyleHTMLAttributes<T> {
      jsx?: boolean;
      global?: boolean;
    }
  }
}

export namespace JSX {
  type XULElementBase = Omit<
    SolidJSX.HTMLAttributes<HTMLElement>,
    | "onmouseover"
    | "onmouseout"
    | "oncontextmenu"
    | "ondragstart"
    | "onkeydown"
    | "onclick"
  > & {
    flex?: `${number}`;
    "data-l10n-id"?: string;
    align?: "center";
    crop?: string;
    context?: string;
    // XUL string event handlers (XUL evaluates string attributes as JS)
    onmouseover?: string | ((e: Event) => void);
    onmouseout?: string | ((e: Event) => void);
    oncontextmenu?: string | ((e: Event) => void);
    ondragstart?: string | ((e: DragEvent) => void);
    onkeydown?: string | ((e: KeyboardEvent) => void);
    onclick?: string | ((e: MouseEvent) => void);
  };
  interface XULBrowserElement extends XULElementBase {
    contextmenu?: string;
    message?: string;
    messagemanagergroup?: string;
    type?: "content";
    remote?: `${boolean}`;
    maychangeremoteness?: `${boolean}`;
    initiallyactive?: string;

    autocompletepopup?: string;
    src?: string;
    disablefullscreen?: `${boolean}`;
    disablehistory?: `${boolean}`;
    nodefaultsrc?: string;
    tooltip?: string;
    xmlns?: string;
    autoscroll?: `${boolean}`;
    disableglobalhistory?: `${boolean}`;
    initialBrowsingContextGroupId?: `${number}`;
    usercontextid?: `${number}`;
    changeuseragent?: `${boolean}`;
    context?: string;
  }

  interface XULMenuListElement extends XULElementBase {
    label?: string;
    accesskey?: string;
    oncommand?: string;
    onCommand?: () => void;
    value?: string;
  }

  interface XULMenuitemElement extends XULElementBase {
    label?: string;
    accesskey?: string;
    type?: "checkbox";
    checked?: boolean;
    disabled?: boolean;
    command?: string;
    oncommand?: string;
    onCommand?: () => void;
    value?: string;
    closemenu?: "none" | "all" | "current" | "parent";
  }

  interface XULRichListItem extends XULElementBase {
    value?: string;
    helpTopic?: string;
  }

  interface XULPopupSetElement extends XULElementBase {
    onpopupshowing?: string | (() => void);
  }

  interface XULMenuPopupElement extends XULElementBase {
    level?: "top" | "parent" | "floating";
    position?:
      | "after_start"
      | "bottomright topright"
      | "end_before"
      | "bottomleft topleft"
      | "overlap";
    onpopupshowing?: string;
    onpopuphiding?: string;
    onPopupShowing?: (e: Event) => void;
    onPopupHiding?: (e: Event) => void;
  }

  interface XULPanelElement extends XULElementBase {
    type?: "arrow";
    noautofocus?: boolean;
    onViewHiding?: () => void;
    position?:
      | "after_start"
      | "end_before"
      | "bottomleft topleft"
      | "bottomright topright"
      | "overlap";
    onPopupShowing?: () => void;
    onPopupHiding?: (event: Event) => void;
    onpopupshowing?: string;
  }

  interface XULMenuElement extends XULElementBase {
    label?: string;
    accesskey?: string;
    onpopupshowing?: string | ((event: Event) => void);
  }

  interface XULBoxElement extends XULElementBase {
    pack?: string;
    orient?: "horizontal" | "vertical";
    popup?: string;
    clicktoscroll?: boolean;
    disablekeynav?: string;
    type?: string;
    mainViewId?: string;
  }

  interface XULToolbarButtonElement extends XULElementBase {
    label?: string;
    accesskey?: string;
    command?: string;
    oncommand?: string;
    onCommand?: () => void;
    context?: string;
    image?: string;
    hidden?: boolean;
    closemenu?: "none" | "all" | "current" | "parent";
  }

  interface XULButtonElement extends XULElementBase {
    label?: string;
    accesskey?: string;
    type?: "menu" | "button";
    oncommand?: string;
    onCommand?: () => void;
    context?: string;
    image?: string;
    disabled?: boolean;
  }

  interface XULImageElement extends XULElementBase {
    src?: string;
    width?: string;
    height?: string;
  }

  interface XULTabElement
    extends XULElementBase, SolidJSX.HTMLAttributes<HTMLElement> {
    onwheel?: SolidJSX.EventHandlerUnion<HTMLElement, WheelEvent>;
  }

  interface IntrinsicElements extends SolidJSX.IntrinsicElements {
    "xul:arrowscrollbox": XULElementBase;
    "xul:browser": XULBrowserElement;
    "xul:button": XULButtonElement;
    "xul:menuitem": XULMenuitemElement;
    "xul:window": XULElementBase;
    "xul:linkset": XULElementBase;
    "xul:popupset": XULPopupSetElement;
    "xul:tooltip": XULElementBase;
    "xul:toolbaritem": XULElementBase & {
      role?: string;
      ariaLabel?: string;
      ariaLevel?: number;
      orient?: "vertical" | "horizontal";
      smoothscroll?: boolean;
      flatList?: boolean;
      tooltip?: string;
      context?: string;
    };
    "xul:tab": XULTabElement;
    "xul:panel": XULPanelElement;
    "xul:panelview": XULPanelElement;
    "xul:menupopup": XULMenuPopupElement;
    "xul:menulist": XULMenuListElement;
    "xul:vbox": XULBoxElement;
    "xul:hbox": XULBoxElement;
    "xul:box": XULElementBase;
    "xul:toolbar": {
      id?: string;
      toolbarname?: string;
      customizable?: string;
      mode?: string;
      context?: string;
      accesskey?: string;
      style?: string | Record<string, string>;
      class?: string;
      children: Element;
    };
    "xul:toolbarbutton": XULToolbarButtonElement;
    "xul:toolbarseparator": XULElementBase;
    "xul:spacer": XULElementBase;
    "xul:splitter": XULElementBase;
    "xul:menuseparator": XULElementBase;
    "xul:menu": XULMenuElement;
    "xul:keyset": {
      id?: string;
      children: Element;
    };
    "xul:key": {
      id?: string;
      "data-l10n-id"?: string;
      "data-l10n-attrs"?: string;
      modifiers?: string;
      keycode?: string;
      key?: string;
      command: string;
    };
    "xul:commandset": {
      id?: string;
      children: Element;
    };
    "xul:command": {
      id: string;
      oncommand: string | (() => void);
    };
    "xul:description": XULElementBase;
    "xul:checkbox": XULElementBase;
    "xul:richlistitem": XULRichListItem;
    "xul:richlistbox": XULElementBase;
    "xul:image": XULImageElement;
    "xul:label": XULElementBase;
    "xul:stack": XULElementBase;

    // Short aliases (without xul: prefix) used in some components
    vbox: XULBoxElement;
    hbox: XULBoxElement;
    box: XULElementBase;
    description: XULElementBase;
    stack: XULElementBase;
    linkset: XULElementBase;
    toolbarseparator: XULElementBase;
    richlistbox: XULElementBase;
    // HTML elements used in XUL context with XUL-specific attributes
    button: Omit<
      SolidJSX.ButtonHTMLAttributes<HTMLButtonElement>,
      "oncommand"
    > & {
      oncommand?: string;
      pack?: string;
      flex?: `${number}`;
    };
  }

  interface Directives {
    dndzone: SolidJSX.Accessor<unknown[]>;
  }
}
