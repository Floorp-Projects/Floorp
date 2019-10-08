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
  interface ChromeUtils {
    /**
     * Use a JSDoc import declaration to pull in the correct type.
     */
    import: (path: string) => any;
    createObjectIn: (content: ContentWindow) => object;
    exportFunction: (fn: Function, scope: object, options?: object) => void;
    cloneInto: (value: any, scope: object, options?: object) => void;
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
    messageManager: MessageManager;
  }

  interface BrowserTab {
    linkedBrowser: Browser;
  }

  interface ChromeWindow {
    gBrowser: Browser;
  }

  type GetPref<T> = (prefName: string, defaultValue?: T) => T;
  type SetPref<T> = (prefName: string, value?: T) => T;

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
    scriptSecurityManager: any;
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

  const Services: Services;
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

declare module "resource://gre/modules/osfile.jsm" {
  export = MockedExports.osfileJSM;
}

declare module "resource://gre/modules/AppConstants.jsm" {
  export = MockedExports.AppConstantsJSM;
}

declare module "resource://gre/modules/ProfilerGetSymbols.jsm" {
  export = MockedExports.ProfilerGetSymbolsJSM;
}

declare var ChromeUtils: MockedExports.ChromeUtils;
declare var Cu: MockedExports.ChromeUtils;

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

declare interface ChromeWindow extends Window {}
