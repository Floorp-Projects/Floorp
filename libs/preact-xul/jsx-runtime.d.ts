import { JSX } from "preact";

declare module "preact" {
  namespace JSX {
    type XULElementBase = JSX.HTMLAttributes<HTMLElement> & {
      // layout
      flex?: `${number}`;
      pack?: string;
      orient?: "horizontal" | "vertical";
      align?: string; // e.g. "center", "baseline", etc.

      // localization
      "data-l10n-id"?: string;
      "data-l10n-name"?: string;
      "data-l10n-args"?: string;

      // common XUL attributes
      href?: string;
      is?: string;
      useoriginprincipal?: string | boolean;
      windowtype?: string;
      hidden?: boolean | string;
      selected?: string | boolean;
      pinned?: string | boolean;
      busy?: string | boolean;
      fadein?: string | boolean;
      value?: string;
      crop?: string;
      src?: string;
      role?: string;
      accesskey?: string;
      tooltip?: string;

      // ui/css helpers
      class?: string;

      // allow additional XUL-specific attributes without explicit typing
      [key: string]: any;
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
      oncommand?: string;
      onCommand?: () => void;
      value?: string;
    }

    interface XULRichListItem extends XULElementBase {
      value?: string;
      helpTopic?: string;
    }

    interface XULPopupSetElement extends XULElementBase {
      onpopupshowing?: string | (() => void);
    }

    interface XULMenuPopupElement extends XULElementBase {
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
      position?:
        | "after_start"
        | "end_before"
        | "bottomleft topleft"
        | "bottomright topright"
        | "overlap";
      onPopupShowing?: () => void;
      onPopupHiding?: () => void;
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
    }

    interface XULToolbarButtonElement extends XULElementBase {
      label?: string;
      accesskey?: string;
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
      oncommand?: string;
      onCommand?: () => void;
      context?: string;
      image?: string;
    }

    interface XULImageElement extends XULElementBase {
      src?: string;
      width?: string;
      height?: string;
    }

    interface XULTabElement extends XULElementBase {
      onwheel?: (e: WheelEvent) => void;
    }

    interface IntrinsicElements {
      "xul:arrowscrollbox": XULElementBase;
      "xul:browser": XULBrowserElement;
      "xul:button": XULButtonElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": XULElementBase;
      "xul:div": XULElementBase;
      "xul:stack": XULElementBase;
      "xul:tabs": XULElementBase;
      "xul:tab": XULTabElement;
      "xul:richlistbox": XULElementBase;
      "xul:richlistitem": XULRichListItem;
      "xul:menubar": XULElementBase;
      "xul:menupopup": XULMenuPopupElement;
      "xul:menuseparator": XULElementBase;
      "xul:menulist": XULMenuListElement;
      "xul:menu": XULMenuElement;
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
      "xul:panel": XULPanelElement;
      "xul:panelview": XULPanelElement;
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
        style?: string;
        class?: string;
        children?: preact.ComponentChildren;
      };
      "xul:toolbarbutton": XULToolbarButtonElement;
      "xul:toolbarseparator": XULElementBase;
      "xul:spacer": XULElementBase;
      "xul:splitter": XULElementBase;
      "xul:keyset": {
        id?: string;
        children?: preact.ComponentChildren;
      };
      "xul:key": {
        id?: string;
        "data-l10n-id"?: string;
        "data-l10n-attrs"?: string;
        modifiers?: string;
        keycode?: string;
        key?: string;
        command?: string;
      };
      "xul:commandset": {
        id?: string;
        children?: preact.ComponentChildren;
      };
      "xul:command": {
        id: string;
        oncommand: string | (() => void);
      };
      "xul:description": XULElementBase;
      "xul:checkbox": XULElementBase;
      "xul:image": XULImageElement;
      "xul:label": XULElementBase;
    }
  }
}
