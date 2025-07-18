/**
 * Gecko generic/specialized adjustments for xpcom and webidl types.
 */

// More specific types for parent process browsing contexts.
interface CanonicalBrowsingContext extends LoadContextMixin {
  embedderElement: XULBrowserElement;
  currentWindowContext: WindowGlobalParent;
  parent: CanonicalBrowsingContext;
  parentWindowContext: WindowGlobalParent;
  top: CanonicalBrowsingContext;
  topWindowContext: WindowGlobalParent;
}

interface ChromeWindow extends Window {
  isChromeWindow: true;
}

interface Document {
  createXULElement(name: "browser"): XULBrowserElement;
}

interface MessageListenerManagerMixin {
  // Overloads that define `data` arg as required, since it's ~always expected.
  addMessageListener(
    msg: string,
    listener: { receiveMessage(_: ReceiveMessageArgument & { data }) },
  );
  removeMessageListener(
    msg: string,
    listener: { receiveMessage(_: ReceiveMessageArgument & { data }) },
  );
}

interface MozQueryInterface {
  <T>(iid: T): nsQIResult<T>;
}

interface nsICryptoHash extends nsISupports {
  // Accepts a TypedArray.
  update(aData: ArrayLike<number>, aLen: number): void;
}

interface nsIDOMWindow extends Window {}

interface nsISimpleEnumerator extends Iterable<any> {}

interface nsISupports {
  wrappedJSObject?: object;
}

interface nsIXPCComponents_Constructor {
  <const T, IIDs = nsIXPCComponents_Interfaces>(cid, id: T, init?): {
    new (...any): nsQIResult<T extends keyof IIDs ? IIDs[T] : T>;
    (...any): nsQIResult<T extends keyof IIDs ? IIDs[T] : T>;
  };
}

interface nsIXPCComponents_Exception {
  (...args: ConstructorParameters<typeof Error>): Error;
}

interface nsIXPCComponents_utils_Sandbox {
  (
    principal: nsIPrincipal | nsIPrincipal[],
    options: object,
  ): typeof globalThis;
}

interface nsXPCComponents_Classes {
  [cid: string]: {
    createInstance<T>(aID: T): nsQIResult<T>;
    getService<T>(aID?: T): unknown extends T ? nsISupports : nsQIResult<T>;
  };
}

// Generic overloads.
interface nsXPCComponents_Utils {
  cloneInto<T>(value: T, ...any): T;
  createObjectIn<T = object>(_, object?: T): T;
  exportFunction<T>(func: T, ...any): T;
  getWeakReference<T>(value: T): { get(): T };
  waiveXrays<T>(object: T): T;
}

// TODO: remove after next TS update.
interface PromiseConstructor {
  withResolvers<T>(): {
    promise: Promise<T>;
    resolve: (value: T | PromiseLike<T>) => void;
    reject: (reason?: any) => void;
  };
}

// Hand-crafted artisanal types.
// Note: src is required in XULElement, so make it required here to fix the interface extension error.
interface XULBrowserElement extends XULElement {
  contextmenu?: string;
  message?: string;
  messagemanagergroup?: string;
  type?: "content";
  remote?: `${boolean}`;
  maychangeremoteness?: `${boolean}`;
  initiallyactive?: string;

  autocompletepopup?: string;
  src: string;
  disablefullscreen?: `${boolean}`; // TODO: remove this
  disablehistory?: `${boolean}`;
  nodefaultsrc?: string;
  tooltip: string;
  xmlns?: string;
  autoscroll?: `${boolean}`;
  disableglobalhistory?: `${boolean}`;
  initialBrowsingContextGroupId?: `${number}`;
  usercontextid?: `${number}`;
  changeuseragent?: `${boolean}`;
  context?: string;
  currentURI?: string;
  docShellIsActive?: boolean;
  isRemoteBrowser?: boolean;
  remoteType?: string;
  close?: () => void;
  loadURI?: (url: unknown, options: {
    loadType?: number;
    referrerPolicy?: string;
    triggeringPrincipal?: unknown;
    triggeringRemoteAddress?: string;
    allowThirdPartyFixup?: boolean;
  }) => void;
  browsingContext: {
    currentWindowGlobal: {
      getActor: (name: string) => {
        sendQuery: (query: string) => string;
      };
    };
  };
}

// https://github.com/microsoft/TypeScript-DOM-lib-generator/issues/1736
interface Localization {
  formatValuesSync(aKeys: L10nKey[]): (string | null)[];
}
