/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TS-TODO - Needs typing.
 *
 * This file contains type stubs for loading things from Gecko. All of these
 * types should be used in the correct places eventually.
 */

/**
 * Namespace anything that has its types mocked out here. These definitions are
 * only "good enough" to get the type checking to pass in this directory.
 * Eventually some more structured solution should be found. This namespace is
 * global and makes sure that all the definitions inside do not clash with
 * naming.
 */
declare namespace MockedExports {

  /**
   * This interface teaches ChromeUtils.import how to find modules.
   */
  interface KnownModules {
    "resource://gre/modules/Services.jsm":
      typeof import("resource://gre/modules/Services.jsm");
    "Services":
      typeof import("Services");
    "chrome":
      typeof import("chrome");
    "resource://gre/modules/osfile.jsm":
      typeof import("resource://gre/modules/osfile.jsm");
    "resource://gre/modules/AppConstants.jsm":
      typeof import("resource://gre/modules/AppConstants.jsm");
    "resource://gre/modules/ProfilerGetSymbols.jsm":
      typeof import("resource://gre/modules/ProfilerGetSymbols.jsm");
    "resource:///modules/CustomizableUI.jsm":
      typeof import("resource:///modules/CustomizableUI.jsm")
    "resource:///modules/CustomizableWidgets.jsm":
      typeof import("resource:///modules/CustomizableWidgets.jsm");
    "resource://devtools/shared/Loader.jsm":
      typeof import("resource://devtools/shared/Loader.jsm");
    "resource://devtools/client/performance-new/popup/background.jsm.js":
      typeof import("resource://devtools/client/performance-new/popup/background.jsm.js");
    "resource://devtools/client/shared/browser-loader.js": any;
    "resource://devtools/client/performance-new/popup/menu-button.jsm.js":
      typeof import("devtools/client/performance-new/popup/menu-button.jsm.js");
    "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js":
      typeof import("devtools/client/performance-new/typescript-lazy-load.jsm.js");
    "resource://devtools/client/performance-new/popup/panel.jsm.js":
      typeof import("devtools/client/performance-new/popup/panel.jsm.js");
    "resource://devtools/client/performance-new/symbolication.jsm.js":
      typeof import("resource://devtools/client/performance-new/symbolication.jsm.js");
    "resource:///modules/PanelMultiView.jsm":
      typeof import("resource:///modules/PanelMultiView.jsm");
  }

  interface ChromeUtils {
    /**
     * This function reads the KnownModules and resolves which import to use.
     * If you are getting the TS2345 error:
     *
     *  Argument of type '"resource:///.../file.jsm"' is not assignable to parameter
     *  of type
     *
     * Then add the file path to the KnownModules above.
     */
    import: <S extends keyof KnownModules>(module: S) => KnownModules[S];
    defineModuleGetter: (target: any, variable: string, path: string) => void;
  }

  interface MessageManager {
    loadFrameScript(url: string, flag: boolean): void;
    sendAsyncMessage: (event: string, data: any) => void;
    addMessageListener: (event: string, listener: (event: any) => void) => void;
  }

  interface Browser {
    addWebTab: (url: string, options: any) => BrowserTab;
    contentPrincipal: any;
    selectedTab: BrowserTab;
    selectedBrowser?: ChromeBrowser;
    messageManager: MessageManager;
    ownerDocument?: ChromeDocument;
  }

  interface BrowserTab {
    linkedBrowser: Browser;
  }

  interface ChromeWindow {
    gBrowser: Browser;
    focus: () => void;
  }

  interface ChromeBrowser {
    browsingContext?: BrowsingContext;
  }

  interface BrowsingContext {
    /**
     * A unique identifier for the browser element that is hosting this
     * BrowsingContext tree. Every BrowsingContext in the element's tree will
     * return the same ID in all processes and it will remain stable regardless of
     * process changes. When a browser element's frameloader is switched to
     * another browser element this ID will remain the same but hosted under the
     * under the new browser element.
     * We are using this identifier for getting the active tab ID and passing to
     * the profiler back-end. See `getActiveBrowserID` for the usage.
     */
    browserId: number;
  }

  type GetPref<T> = (prefName: string, defaultValue?: T) => T;
  type SetPref<T> = (prefName: string, value?: T) => T;

  interface nsIURI {}

  type Services = {
    prefs: {
      clearUserPref: (prefName: string) => void;
      getStringPref: GetPref<string>;
      setStringPref: SetPref<string>;
      getCharPref: GetPref<string>;
      setCharPref: SetPref<string>;
      getIntPref: GetPref<number>;
      setIntPref: SetPref<number>;
      getBoolPref: GetPref<boolean>;
      setBoolPref: SetPref<boolean>;
      addObserver: any;
      removeObserver: any;
    };
    profiler: any;
    platform: string;
    obs: {
      addObserver: (observer: object, type: string) => void;
      removeObserver: (observer: object, type: string) => void;
    };
    wm: {
      getMostRecentWindow: (name: string) => ChromeWindow;
    };
    focus: {
      activeWindow: ChromeWindow;
    };
    io: {
      newURI(url: string): nsIURI;
    },
    scriptSecurityManager: any;
    startup: {
      quit: (optionsBitmask: number) => void,
      eForceQuit: number,
      eRestart: number
    };
  };

  const ServicesJSM: {
    Services: Services;
  };

  const EventEmitter: {
    decorate: (target: object) => void;
  };

  const ProfilerGetSymbolsJSM: {
    ProfilerGetSymbols: {
      getSymbolTable: (
        path: string,
        debugPath: string,
        breakpadId: string
      ) => any;
    };
  };

  const AppConstantsJSM: {
    AppConstants: {
      platform: string;
    };
  };

  const osfileJSM: {
    OS: {
      Path: {
        split: (
          path: string
        ) => {
          absolute: boolean;
          components: string[];
          winDrive?: string;
        };
        join: (...pathParts: string[]) => string;
      };
      File: {
        stat: (path: string) => Promise<{ isDir: boolean }>;
        Error: any;
      };
    };
  };

  interface BrowsingContextStub {}
  interface PrincipalStub {}

  interface WebChannelTarget {
    browsingContext: BrowsingContextStub,
    browser: Browser,
    eventTarget: null,
    principal: PrincipalStub,
  }

  const WebChannelJSM: any;

  // TS-TODO
  const CustomizableUIJSM: any;
  const CustomizableWidgetsJSM: any;
  const PanelMultiViewJSM: any;

  const LoaderJSM: {
    require: (path: string) => any;
  };

  const Services: Services;

  // This class is needed by the Cc importing mechanism. e.g.
  // Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  class nsIFilePicker {}

  interface FilePicker {
    init: (window: Window, title: string, mode: number) => void;
    open: (callback: (rv: number) => unknown) => void;
    // The following are enum values.
    modeGetFolder: number;
    returnOK: number;
    file: {
      path: string
    }
  }

  // This class is needed by the Cc importing mechanism. e.g.
  // Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  class nsIEnvironment {}

  interface Environment {
    exists(envName: string): boolean;
    get(envName: string): string;
    set(envName: string, value: string): void;
  }

  interface Cc {
    "@mozilla.org/process/environment;1": {
      getService(service: nsIEnvironment): Environment
    },
    "@mozilla.org/filepicker;1": {
      createInstance(instance: nsIFilePicker): FilePicker
    }
  }

  interface Ci {
    nsIFilePicker: nsIFilePicker;
    nsIEnvironment: nsIEnvironment;
  }

  interface Cu {
    /**
     * This function reads the KnownModules and resolves which import to use.
     * If you are getting the TS2345 error:
     *
     *  Argument of type '"resource:///.../file.jsm"' is not assignable to parameter
     *  of type
     *
     * Then add the file path to the KnownModules above.
     */
    import: <S extends keyof KnownModules>(module: S) => KnownModules[S];
    createObjectIn: (content: ContentWindow) => object;
    exportFunction: (fn: Function, scope: object, options?: object) => void;
    cloneInto: (value: any, scope: object, options?: object) => void;
    isInAutomation: boolean;
  }

  const chrome: {
    Cc: Cc,
    Ci: Ci,
    Cu: Cu,
  };
}


declare module "devtools/client/shared/vendor/react" {
  import * as React from "react";
  export = React;
}

declare module "devtools/client/shared/vendor/react-dom-factories" {
  import * as ReactDomFactories from "react-dom-factories";
  export = ReactDomFactories;
}

declare module "devtools/client/shared/vendor/redux" {
  import * as Redux from "redux";
  export = Redux;
}

declare module "devtools/client/shared/vendor/react-redux" {
  import * as ReactRedux from "react-redux";
  export = ReactRedux;
}

declare module "devtools/shared/event-emitter2" {
  export = MockedExports.EventEmitter;
}

declare module "resource://gre/modules/Services.jsm" {
  export = MockedExports.ServicesJSM;
}

declare module "Services" {
  export = MockedExports.Services;
}

declare module "chrome" {
  export = MockedExports.chrome;
}

declare module "ChromeUtils" {
  export = ChromeUtils;
}

declare module "resource://gre/modules/osfile.jsm" {
  export = MockedExports.osfileJSM;
}

declare module "resource://gre/modules/AppConstants.jsm" {
  export = MockedExports.AppConstantsJSM;
}

declare module "resource://gre/modules/ProfilerGetSymbols.jsm" {
  export = MockedExports.ProfilerGetSymbolsJSM;
}

declare module "resource://gre/modules/WebChannel.jsm" {
  export = MockedExports.WebChannelJSM;
}

declare module "resource://devtools/client/performance-new/popup/background.jsm.js" {
  import * as Background from "devtools/client/performance-new/popup/background.jsm.js";
  export = Background
}

declare module "resource://devtools/client/performance-new/symbolication.jsm.js" {
  import * as PerfSymbolication from "devtools/client/performance-new/symbolication.jsm.js";
  export = PerfSymbolication
}

declare module "resource:///modules/CustomizableUI.jsm" {
  export = MockedExports.CustomizableUIJSM;
}

declare module "resource:///modules/CustomizableWidgets.jsm" {
  export = MockedExports.CustomizableWidgetsJSM;
}

declare module "resource:///modules/PanelMultiView.jsm" {
  export = MockedExports.PanelMultiViewJSM;
}

declare module "resource://devtools/shared/Loader.jsm" {
  export = MockedExports.LoaderJSM;
}

declare var ChromeUtils: MockedExports.ChromeUtils;

// These global objects can be used directly in JSM files only.
// In a CommonJS context you need to import them with `require("chrome")`.
declare var Cu: MockedExports.Cu;
declare var Cc: MockedExports.Cc;
declare var Ci: MockedExports.Ci;

/**
 * This is a variant on the normal Document, as it contains chrome-specific properties.
 */
declare interface ChromeDocument extends Document {
  /**
   * Create a XUL element of a specific type. Right now this function
   * only refines iframes, but more tags could be added.
   */
  createXULElement: ((type: "iframe") => XULIframeElement) &
    ((type: string) => XULElement);
}

/**
 * This is a variant on the HTMLElement, as it contains chrome-specific properties.
 */
declare interface ChromeHTMLElement extends HTMLElement {
  ownerDocument: ChromeDocument;
}

declare interface XULElement extends HTMLElement {
  ownerDocument: ChromeDocument;
}

declare interface XULIframeElement extends XULElement {
  contentWindow: ChromeWindow;
  src: string;
}

declare interface ChromeWindow extends Window {
  openWebLinkIn: (
    url: string,
    where: "current" | "tab" | "tabshifted" | "window" | "save",
    // TS-TODO
    params?: unknown
  ) => void;
  openTrustedLinkIn: (
    url: string,
    where: "current" | "tab" | "tabshifted" | "window" | "save",
    // TS-TODO
    params?: unknown
  ) => void;
}

declare interface MenuListElement extends XULElement {
  value: string;
  disabled: boolean;
}

declare interface XULCommandEvent extends Event {
  target: XULElement
}

declare interface XULElementWithCommandHandler {
  addEventListener: (type: "command", handler: (event: XULCommandEvent) => void, isCapture?: boolean) => void
  removeEventListener: (type: "command", handler: (event: XULCommandEvent) => void, isCapture?: boolean) => void
}
