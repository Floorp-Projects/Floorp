import type { JSX as SolidJSX } from "solid-js";

declare module "solid-js" {
  namespace JSX {
    type XULElementBase = SolidJSX.HTMLAttributes<HTMLElement> & {
      flex?: `${number}`;
      "data-l10n-id"?: string;
      align?: "center";
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
    interface XULMenuitemElement extends XULElementBase {
      label?: string;
      accesskey?: string;
      type?: "checkbox";
      checked?: boolean;
      onCommand?: () => void;
    }

    interface XULRichListItem extends XULElementBase {
      value?: string;
      helpTopic?: string;
    }

    interface IntrinsicElements {
      "xul:browser": XULBrowserElement;
      "xul:menuitem": XULMenuitemElement;
      "xul:window": XULElementBase;
      "xul:linkset": XULElementBase;

      "xul:popupset": XULElementBase;
      "xul:tooltip": XULElementBase;
      "xul:panel": XULElementBase;
      "xul:menupopup": XULElementBase;
      "xul:vbox": XULElementBase;
      "xul:hbox": XULElementBase;
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
        children: Element;
      };
      "xul:toolbarbutton": XULElementBase;
      "xul:spacer": XULElementBase;
      "xul:splitter": XULElementBase;
      "xul:menuseparator": XULElementBase;
      "xul:menu": XULElementBase;
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
      "xul:image": XULElementBase;
      "xul:label": XULElementBase;
    }

    interface Directives {
      dndzone: Accessor<unknown[]>;
    }
  }
}
