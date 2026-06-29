// SPDX-License-Identifier: MPL-2.0

type TaskFunction = () => void | Promise<void>;
type CleanupFunction = () => void | Promise<void>;
type ConditionFunction = () => unknown | Promise<unknown>;
type EventCheckFunction = (event: unknown) => unknown | Promise<unknown>;
type MutationCheckFunction = () => unknown;

type GlobalName =
  | "add_task"
  | "registerCleanupFunction"
  | "ok"
  | "is"
  | "isnot"
  | "info"
  | "todo"
  | "Assert"
  | "TestUtils"
  | "BrowserTestUtils"
  | "makeURI"
  | "gBrowser"
  | "gBrowserInit";

type PreviousGlobal = {
  existed: boolean;
  value: unknown;
};

type RegisteredTask = {
  name: string;
  fn: TaskFunction;
};

const GLOBAL_NAMES: GlobalName[] = [
  "add_task",
  "registerCleanupFunction",
  "ok",
  "is",
  "isnot",
  "info",
  "todo",
  "Assert",
  "TestUtils",
  "BrowserTestUtils",
  "makeURI",
  "gBrowser",
  "gBrowserInit",
];

const objectHasOwn = Object.prototype.hasOwnProperty;

function formatValue(value: unknown): string {
  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
}

function taskName(fn: TaskFunction, index: number): string {
  return fn.name || `task ${index + 1}`;
}

function usesMozillaBorrowedStyle(file: string): boolean {
  return /\.(?:js|mjs|jsx)$/i.test(file);
}

type WindowMediatorLike = {
  getMostRecentWindow(windowType: string): unknown;
  getMostRecentBrowserWindow?(): unknown;
  getEnumerator?(windowType: string): {
    hasMoreElements: () => boolean;
    getNext: () => unknown;
  };
};

type ServicesLike = {
  io?: {
    newURI: (uri: string) => unknown;
  };
  scriptSecurityManager?: {
    getSystemPrincipal: () => unknown;
  };
  wm?: WindowMediatorLike;
};

type BrowserChromeWindowLike = {
  closed?: boolean;
  gBrowser?: unknown;
  gBrowserInit?: unknown;
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object";
}

function isIterable(value: unknown): value is Iterable<unknown> {
  if (!isRecord(value)) {
    return false;
  }
  const maybeIterator = (value as { [Symbol.iterator]?: unknown })[
    Symbol.iterator
  ];
  return typeof maybeIterator === "function";
}

function wait(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForCondition(
  predicate: () => boolean,
  message: string,
  timeoutMs = 5000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      return;
    }
    await wait(25);
  }
  if (!predicate()) {
    throw new Error(message);
  }
}

async function waitForMozillaCondition(
  condition: ConditionFunction,
  message = "waitForCondition",
  intervalMs = 100,
  maxTries = 50,
): Promise<unknown> {
  if (typeof condition !== "function") {
    throw new Error("waitForCondition requires a condition function");
  }

  const interval = Number.isFinite(intervalMs) && intervalMs >= 0
    ? intervalMs
    : 100;
  const tries = Number.isFinite(maxTries) && maxTries > 0
    ? Math.floor(maxTries)
    : 50;

  for (let index = 0; index < tries; index++) {
    let result: unknown;
    try {
      result = await condition();
    } catch (error) {
      throw new Error(
        `${message} - threw exception: ${
          error instanceof Error ? error.message : formatValue(error)
        }`,
      );
    }

    if (result) {
      return result;
    }

    if (index < tries - 1) {
      await wait(interval);
    }
  }

  throw new Error(`${message} - timed out after ${tries} tries.`);
}

function servicesFromGlobals(globals: Record<string, unknown>): ServicesLike {
  const existingServices = globals.Services;
  if (isRecord(existingServices)) {
    return existingServices as ServicesLike;
  }

  const chromeUtils = globals.ChromeUtils;
  if (!isRecord(chromeUtils)) {
    return {};
  }

  const importESModule = chromeUtils.importESModule;
  if (typeof importESModule !== "function") {
    return {};
  }

  try {
    const imported = importESModule(
      "resource://gre/modules/Services.sys.mjs",
    ) as unknown;
    if (isRecord(imported) && isRecord(imported.Services)) {
      return imported.Services as ServicesLike;
    }
  } catch {
    // Browser-chrome compatibility is best-effort; assertion globals still work.
  }

  return {};
}

function mostRecentBrowserWindow(
  globals: Record<string, unknown>,
): BrowserChromeWindowLike {
  const services = servicesFromGlobals(globals);
  const maybeRecentWindow = waiveXrays(
    services.wm?.getMostRecentBrowserWindow?.() ??
      services.wm?.getMostRecentWindow("navigator:browser"),
    globals,
  );
  if (isHealthyBrowserWindow(maybeRecentWindow, globals)) {
    return maybeRecentWindow as BrowserChromeWindowLike;
  }

  const enumerator = services.wm?.getEnumerator?.("navigator:browser");
  let fallbackWindow: BrowserChromeWindowLike | undefined;
  if (enumerator) {
    while (enumerator.hasMoreElements()) {
      const maybeWindow = waiveXrays(enumerator.getNext(), globals);
      if (isHealthyBrowserWindow(maybeWindow, globals)) {
        fallbackWindow = maybeWindow as BrowserChromeWindowLike;
      }
    }
  }

  return fallbackWindow ?? {};
}

function isHealthyBrowserWindow(
  value: unknown,
  globals: Record<string, unknown>,
): value is BrowserChromeWindowLike {
  const maybeWindow = waiveXrays(value, globals);
  if (!isRecord(maybeWindow) || maybeWindow.closed) {
    return false;
  }
  return isHealthyGBrowser(maybeWindow.gBrowser, globals);
}

function isHealthyGBrowser(
  value: unknown,
  globals: Record<string, unknown>,
): value is Record<string, unknown> {
  const gBrowser = waiveXrays(value, globals);
  if (!isRecord(gBrowser)) {
    return false;
  }
  if (
    typeof gBrowser.addTab !== "function" ||
    typeof gBrowser.removeTab !== "function"
  ) {
    return false;
  }
  const tabs = tabList(gBrowser);
  if (tabs.length === 0) {
    return false;
  }
  if (isRecord(waiveXrays(gBrowser.selectedBrowser, globals))) {
    return true;
  }
  return isRecord(browserForTab(gBrowser, gBrowser.selectedTab, globals));
}

function waiveXrays(
  value: unknown,
  globals: Record<string, unknown>,
): unknown {
  const chromeUtils = globals.ChromeUtils;
  if (!isRecord(chromeUtils)) {
    return value;
  }

  const waive = chromeUtils.waiveXrays;
  if (typeof waive !== "function") {
    return value;
  }

  try {
    return waive(value);
  } catch {
    return value;
  }
}

function tabList(gBrowser: Record<string, unknown>): unknown[] {
  const tabs = gBrowser.tabs;
  return isIterable(tabs) ? Array.from(tabs) : [];
}

function tabAttribute(
  tab: unknown,
  name: string,
  globals: Record<string, unknown>,
): unknown {
  const tabRecord = waiveXrays(tab, globals);
  if (!isRecord(tabRecord)) {
    return undefined;
  }
  const getAttribute = tabRecord.getAttribute;
  if (typeof getAttribute !== "function") {
    return undefined;
  }
  try {
    return Reflect.apply(getAttribute, tabRecord, [name]);
  } catch {
    return undefined;
  }
}

function sameTab(
  left: unknown,
  right: unknown,
  globals: Record<string, unknown>,
): boolean {
  const leftTab = waiveXrays(left, globals);
  const rightTab = waiveXrays(right, globals);
  if (Object.is(leftTab, rightTab)) {
    return true;
  }
  const leftPanel = tabAttribute(leftTab, "linkedpanel", globals);
  const rightPanel = tabAttribute(rightTab, "linkedpanel", globals);
  if (
    typeof leftPanel === "string" &&
    leftPanel.length > 0 &&
    leftPanel === rightPanel
  ) {
    return true;
  }
  const leftBrowser = isRecord(leftTab)
    ? waiveXrays(leftTab.linkedBrowser, globals)
    : undefined;
  const rightBrowser = isRecord(rightTab)
    ? waiveXrays(rightTab.linkedBrowser, globals)
    : undefined;
  return Boolean(leftBrowser && Object.is(leftBrowser, rightBrowser));
}

function tabListIncludes(
  tabs: unknown[],
  tab: unknown,
  globals: Record<string, unknown>,
): boolean {
  return tabs.some((candidate) => sameTab(candidate, tab, globals));
}

function browserForTab(
  gBrowser: Record<string, unknown>,
  tab: unknown,
  globals: Record<string, unknown>,
): unknown {
  const tabRecord = waiveXrays(tab, globals);
  if (isRecord(tabRecord) && tabRecord.linkedBrowser) {
    return waiveXrays(tabRecord.linkedBrowser, globals);
  }

  const getBrowserForTab = gBrowser.getBrowserForTab;
  if (typeof getBrowserForTab === "function") {
    return waiveXrays(
      Reflect.apply(getBrowserForTab, gBrowser, [tab]),
      globals,
    );
  }

  return undefined;
}

function browserCurrentSpec(
  browser: unknown,
  globals: Record<string, unknown>,
) {
  const browserRecord = waiveXrays(browser, globals);
  if (!isRecord(browserRecord)) {
    return undefined;
  }
  const currentURI = waiveXrays(browserRecord.currentURI, globals);
  if (!isRecord(currentURI)) {
    return undefined;
  }
  return typeof currentURI.spec === "string" ? currentURI.spec : undefined;
}

function browserMatchesURI(
  browser: unknown,
  wanted: unknown,
  globals: Record<string, unknown>,
): boolean {
  if (wanted === undefined || wanted === null || wanted === false) {
    return true;
  }

  const spec = browserCurrentSpec(browser, globals);
  if (typeof wanted === "string") {
    return spec === wanted;
  }

  if (typeof wanted === "function") {
    return Boolean(wanted(spec));
  }

  return true;
}

function callableMethod(
  value: unknown,
  name: string,
): ((...args: unknown[]) => unknown) | undefined {
  const record = isRecord(value) ? value : undefined;
  const method = record?.[name];
  return typeof method === "function"
    ? method as (...args: unknown[]) => unknown
    : undefined;
}

function eventTargetRecord(
  value: unknown,
  globals: Record<string, unknown>,
): Record<string, unknown> | undefined {
  const target = waiveXrays(value, globals);
  return isRecord(target) && callableMethod(target, "addEventListener") &&
      callableMethod(target, "removeEventListener")
    ? target
    : undefined;
}

function waitForEvent(
  target: Record<string, unknown>,
  eventName: string,
  predicate: () => boolean,
  timeoutMs = 5000,
): Promise<void> {
  const addEventListener = callableMethod(target, "addEventListener");
  const removeEventListener = callableMethod(target, "removeEventListener");
  if (!addEventListener || !removeEventListener) {
    return Promise.reject(new Error(`Cannot wait for ${eventName}`));
  }

  return new Promise<void>((resolve, reject) => {
    const cleanup = (): void => {
      clearTimeout(timeoutId);
      try {
        removeEventListener.call(target, eventName, onEvent, true);
      } catch {
        // Event targets may reject late removal during shutdown.
      }
    };

    const onEvent = (): void => {
      if (!predicate()) {
        return;
      }
      cleanup();
      resolve();
    };

    const timeoutId = setTimeout(() => {
      cleanup();
      reject(new Error(`Timed out waiting for ${eventName}`));
    }, timeoutMs);

    try {
      addEventListener.call(target, eventName, onEvent, true);
    } catch (error) {
      cleanup();
      reject(error);
    }
  });
}

function waitForEventCompat(
  targetValue: unknown,
  eventName: string,
  capture: unknown,
  checkFn: unknown,
  wantsUntrusted: unknown,
  globals: Record<string, unknown>,
  timeoutMs = 10000,
): Promise<unknown> {
  const target = eventTargetRecord(targetValue, globals);
  if (!target) {
    return Promise.reject(
      new Error(`BrowserTestUtils.waitForEvent requires ${eventName} target`),
    );
  }

  const addEventListener = callableMethod(target, "addEventListener");
  const removeEventListener = callableMethod(target, "removeEventListener");
  if (!addEventListener || !removeEventListener) {
    return Promise.reject(new Error(`Cannot wait for ${eventName}`));
  }

  const listenerOptions = typeof capture === "boolean" || isRecord(capture)
    ? capture
    : false;
  const eventCheck = typeof checkFn === "function"
    ? checkFn as EventCheckFunction
    : undefined;

  return new Promise<unknown>((resolve, reject) => {
    let settled = false;
    const cleanup = (): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeoutId);
      try {
        removeEventListener.call(
          target,
          eventName,
          onEvent,
          listenerOptions,
        );
      } catch {
        // Event targets may reject late removal during shutdown.
      }
    };

    const settleAsync = (
      callback: () => void,
    ): void => {
      setTimeout(callback, 0);
    };

    const onEvent = (event: unknown): void => {
      Promise.resolve().then(async () => {
        if (settled) {
          return;
        }
        try {
          if (eventCheck && !await eventCheck(event)) {
            return;
          }
          cleanup();
          settleAsync(() => resolve(event));
        } catch (error) {
          cleanup();
          settleAsync(() => reject(error));
        }
      });
    };

    const timeoutId = setTimeout(() => {
      cleanup();
      reject(new Error(`Timed out waiting for ${eventName}`));
    }, timeoutMs);

    try {
      addEventListener.call(
        target,
        eventName,
        onEvent,
        listenerOptions,
        wantsUntrusted,
      );
    } catch (error) {
      cleanup();
      reject(error);
    }
  });
}

type MutationObserverLike = {
  observe: (target: unknown, options: unknown) => void;
  disconnect: () => void;
};

type MutationObserverConstructorLike = new (
  callback: (records: unknown[], observer: MutationObserverLike) => void,
) => MutationObserverLike;

function mutationObserverConstructor(
  targetValue: unknown,
  globals: Record<string, unknown>,
): MutationObserverConstructorLike | undefined {
  const target = waiveXrays(targetValue, globals);
  const targetRecord = isRecord(target) ? target : undefined;
  const ownerGlobal = waiveXrays(
    targetRecord?.documentGlobal ?? targetRecord?.ownerGlobal,
    globals,
  );
  const constructorValue = isRecord(ownerGlobal)
    ? ownerGlobal.MutationObserver
    : globals.MutationObserver;

  if (typeof constructorValue !== "function") {
    return undefined;
  }

  return constructorValue as MutationObserverConstructorLike;
}

function waitForMutationConditionCompat(
  targetValue: unknown,
  options: unknown,
  checkFn: unknown,
  globals: Record<string, unknown>,
  timeoutMs = 10000,
): Promise<unknown> {
  const target = waiveXrays(targetValue, globals);
  if (!isRecord(target)) {
    return Promise.reject(
      new Error("BrowserTestUtils.waitForMutationCondition requires a target"),
    );
  }
  if (typeof checkFn !== "function") {
    return Promise.reject(
      new Error(
        "BrowserTestUtils.waitForMutationCondition requires a condition",
      ),
    );
  }

  const mutationObserver = mutationObserverConstructor(target, globals);
  if (!mutationObserver) {
    return Promise.reject(
      new Error("MutationObserver is unavailable in this test context"),
    );
  }
  const check = checkFn as MutationCheckFunction;

  try {
    const initial = check();
    if (initial) {
      return Promise.resolve(initial);
    }
  } catch (error) {
    return Promise.reject(error);
  }

  return new Promise<unknown>((resolve, reject) => {
    let settled = false;
    let observer: MutationObserverLike | undefined;
    const cleanup = (): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeoutId);
      try {
        observer?.disconnect();
      } catch {
        // Observers may already be disconnected during test teardown.
      }
    };

    const timeoutId = setTimeout(() => {
      cleanup();
      reject(
        new Error("Timed out waiting for BrowserTestUtils mutation condition"),
      );
    }, timeoutMs);

    try {
      observer = new mutationObserver(() => {
        if (settled) {
          return;
        }
        try {
          const value = check();
          if (!value) {
            return;
          }
          cleanup();
          resolve(value);
        } catch (error) {
          cleanup();
          reject(error);
        }
      });
      observer.observe(target, isRecord(options) ? options : {});
    } catch (error) {
      cleanup();
      reject(error);
    }
  });
}

function notificationStack(
  notificationBoxValue: unknown,
  globals: Record<string, unknown>,
): unknown {
  const notificationBox = waiveXrays(notificationBoxValue, globals);
  if (!isRecord(notificationBox)) {
    return undefined;
  }
  return waiveXrays(notificationBox.stack, globals);
}

function notificationValue(
  notification: unknown,
  globals: Record<string, unknown>,
): unknown {
  const notificationRecord = waiveXrays(notification, globals);
  if (!isRecord(notificationRecord)) {
    return undefined;
  }
  const getAttribute = callableMethod(notificationRecord, "getAttribute");
  if (!getAttribute) {
    return undefined;
  }
  try {
    return getAttribute.call(notificationRecord, "value");
  } catch {
    return undefined;
  }
}

function findNotificationInNotificationBox(
  notificationBoxValue: unknown,
  notificationValueExpected: unknown,
  globals: Record<string, unknown>,
): unknown {
  const stack = notificationStack(notificationBoxValue, globals);
  const stackRecord = waiveXrays(stack, globals);
  if (!isRecord(stackRecord)) {
    return undefined;
  }
  const children = stackRecord.children;
  const candidates = isIterable(children) ? Array.from(children) : [];
  return candidates.find((candidate) =>
    notificationValue(candidate, globals) === notificationValueExpected
  );
}

async function waitForNotificationInNotificationBoxCompat(
  notificationBox: unknown,
  notificationValueExpected: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  return await waitForMozillaCondition(
    () =>
      findNotificationInNotificationBox(
        notificationBox,
        notificationValueExpected,
        globals,
      ),
    `notification ${
      String(notificationValueExpected)
    } should be in the notification box`,
    25,
    400,
  );
}

async function waitForTabSwitch(
  gBrowser: Record<string, unknown>,
  tab: unknown,
  globals: Record<string, unknown>,
): Promise<void> {
  const tabContainer = eventTargetRecord(gBrowser.tabContainer, globals);
  const switchEvent = tabContainer
    ? Promise.race([
      waitForEvent(
        tabContainer,
        "TabSwitchDone",
        () => sameTab(gBrowser.selectedTab, tab, globals),
      ),
      waitForEvent(
        tabContainer,
        "TabSelect",
        () => sameTab(gBrowser.selectedTab, tab, globals),
      ),
    ]).catch(() => undefined)
    : Promise.resolve(undefined);

  await waitForCondition(
    () =>
      sameTab(gBrowser.selectedTab, tab, globals) &&
      isRecord(browserForTab(gBrowser, tab, globals)),
    "BrowserTestUtils expected tab switch",
  );
  await Promise.race([switchEvent, wait(100)]);
}

function numberConstant(
  value: unknown,
  name: string,
  fallback: number,
  globals: Record<string, unknown>,
): number {
  const record = waiveXrays(value, globals);
  if (!isRecord(record)) {
    return fallback;
  }
  const raw = record[name];
  return typeof raw === "number" ? raw : fallback;
}

function makeLoadOptions(
  globals: Record<string, unknown>,
  extraOptions: Record<string, unknown> = {},
): Record<string, unknown> {
  const services = servicesFromGlobals(globals);
  const options: Record<string, unknown> = {
    inBackground: false,
    skipAnimation: true,
    ...extraOptions,
  };
  if (!("triggeringPrincipal" in options)) {
    const principal = services.scriptSecurityManager?.getSystemPrincipal();
    if (principal) {
      options.triggeringPrincipal = principal;
    }
  }
  return options;
}

async function waitForNewTab(
  gBrowser: Record<string, unknown>,
  previousTabs: unknown[],
  globals: Record<string, unknown>,
): Promise<unknown> {
  let newTab: unknown;
  await waitForCondition(() => {
    newTab = tabList(gBrowser).find((tab) =>
      !tabListIncludes(previousTabs, tab, globals)
    );
    return Boolean(newTab);
  }, "BrowserTestUtils expected a new tab");
  return waiveXrays(newTab, globals);
}

function addTabCompat(
  gBrowserValue: unknown,
  urlOrOptions: unknown,
  maybeOptions: unknown,
  globals: Record<string, unknown>,
): unknown {
  const gBrowser = waiveXrays(gBrowserValue, globals);
  if (!isRecord(gBrowser) || typeof gBrowser.addTab !== "function") {
    throw new Error("BrowserTestUtils.addTab requires a gBrowser");
  }

  const url = typeof urlOrOptions === "string" ? urlOrOptions : "about:blank";
  const optionsInput = isRecord(maybeOptions)
    ? maybeOptions
    : isRecord(urlOrOptions)
    ? urlOrOptions
    : {};
  const previousTabs = tabList(gBrowser);
  const returnedTab = waiveXrays(
    Reflect.apply(gBrowser.addTab, gBrowser, [
      url,
      makeLoadOptions(globals, optionsInput),
    ]),
    globals,
  );
  if (returnedTab) {
    return returnedTab;
  }

  const newTab = tabList(gBrowser).find((tab) =>
    !tabListIncludes(previousTabs, tab, globals)
  );
  if (newTab) {
    return waiveXrays(newTab, globals);
  }

  throw new Error(
    "BrowserTestUtils.addTab expected gBrowser.addTab to return a tab",
  );
}

async function openNewForegroundTabCompat(
  gBrowserValue: unknown,
  opening: unknown,
  waitForLoad: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  const gBrowser = waiveXrays(gBrowserValue, globals);
  if (!isRecord(gBrowser)) {
    throw new Error(
      "BrowserTestUtils.openNewForegroundTab requires a gBrowser",
    );
  }

  const previousTabs = tabList(gBrowser);
  let tab: unknown;
  if (typeof opening === "function") {
    opening();
    tab = await waitForNewTab(gBrowser, previousTabs, globals);
  } else {
    tab = addTabCompat(gBrowser, opening, undefined, globals);
  }

  const browser = browserForTab(gBrowser, tab, globals);
  const requestedUrl = typeof opening === "string"
    ? opening
    : opening === undefined
    ? "about:blank"
    : undefined;
  const loadPromise = waitForLoad !== false && requestedUrl
    ? browserLoadedCompat(browser, false, requestedUrl, globals)
    : undefined;

  gBrowser.selectedTab = tab;
  await waitForTabSwitch(gBrowser, tab, globals);

  await loadPromise;
  return tab;
}

async function browserLoadedCompat(
  browser: unknown,
  _includeSubFrames: unknown,
  wanted: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  const browserRecord = waiveXrays(browser, globals);
  if (!isRecord(browserRecord)) {
    throw new Error("BrowserTestUtils.browserLoaded requires a browser");
  }

  const waitForAnyLoad = wanted === undefined || wanted === null ||
    wanted === false;
  if (!waitForAnyLoad && browserMatchesURI(browserRecord, wanted, globals)) {
    return browserRecord;
  }

  const browserTarget = eventTargetRecord(browserRecord, globals);
  if (!browserTarget) {
    throw new Error("BrowserTestUtils.browserLoaded requires an event target");
  }

  await waitForEvent(
    browserTarget,
    "load",
    () => waitForAnyLoad || browserMatchesURI(browserRecord, wanted, globals),
    10000,
  );
  return browserRecord;
}

async function browserStoppedCompat(
  browser: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  const browserRecord = waiveXrays(browser, globals);
  if (!isRecord(browserRecord)) {
    throw new Error("BrowserTestUtils.browserStopped requires a browser");
  }

  const webProgress = waiveXrays(browserRecord.webProgress, globals);
  const addProgressListener = callableMethod(
    webProgress,
    "addProgressListener",
  );
  const removeProgressListener = callableMethod(
    webProgress,
    "removeProgressListener",
  );
  if (
    !isRecord(webProgress) || !addProgressListener || !removeProgressListener
  ) {
    throw new Error(
      "BrowserTestUtils.browserStopped requires browser.webProgress",
    );
  }

  const ci = waiveXrays(globals.Ci, globals);
  const webProgressListener = isRecord(ci)
    ? ci.nsIWebProgressListener
    : undefined;
  const webProgressInterface = isRecord(ci) ? ci.nsIWebProgress : undefined;
  const stateStop = numberConstant(
    webProgressListener,
    "STATE_STOP",
    0x10,
    globals,
  );
  const notifyStateAll = numberConstant(
    webProgressInterface,
    "NOTIFY_STATE_ALL",
    0x0f,
    globals,
  );
  const chromeUtils = waiveXrays(globals.ChromeUtils, globals);
  const generateQI = callableMethod(chromeUtils, "generateQI");

  return await new Promise<unknown>((resolve, reject) => {
    let settled = false;
    const timeoutId = setTimeout(() => {
      cleanup();
      reject(new Error("BrowserTestUtils.browserStopped timed out"));
    }, 10000);

    const cleanup = (): void => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeoutId);
      try {
        removeProgressListener.call(webProgress, listener);
      } catch {
        // The listener may already be removed during browser teardown.
      }
    };

    const listener: Record<string, unknown> = {
      onStateChange: (...args: unknown[]): void => {
        const flags = Number(args[2]);
        if (!Number.isFinite(flags) || (flags & stateStop) === 0) {
          return;
        }
        cleanup();
        resolve(browserRecord);
      },
      onProgressChange: (): void => {},
      onLocationChange: (): void => {},
      onStatusChange: (): void => {},
      onSecurityChange: (): void => {},
      onContentBlockingEvent: (): void => {},
    };

    if (generateQI && isRecord(chromeUtils)) {
      try {
        listener.QueryInterface = generateQI.call(chromeUtils, [
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]);
      } catch {
        // Plain JS listener methods are enough in current Floorp test builds.
      }
    }

    try {
      addProgressListener.call(webProgress, listener, notifyStateAll);
    } catch (error) {
      cleanup();
      reject(error);
    }
  });
}

function ownerGlobalFromTab(
  tab: unknown,
  globals: Record<string, unknown>,
): Record<string, unknown> | undefined {
  const tabRecord = waiveXrays(tab, globals);
  if (isRecord(tabRecord)) {
    const ownerGlobal = waiveXrays(tabRecord.ownerGlobal, globals);
    if (isRecord(ownerGlobal)) {
      return ownerGlobal;
    }
  }
  const browserWindow = mostRecentBrowserWindow(globals);
  return isRecord(browserWindow)
    ? browserWindow as Record<string, unknown>
    : {};
}

async function removeTabCompat(
  tab: unknown,
  options: unknown,
  globals: Record<string, unknown>,
): Promise<void> {
  const ownerGlobal = ownerGlobalFromTab(tab, globals);
  const gBrowser = isRecord(ownerGlobal?.gBrowser)
    ? ownerGlobal.gBrowser
    : mostRecentBrowserWindow(globals).gBrowser;
  const browser = waiveXrays(gBrowser, globals);
  if (!isRecord(browser) || typeof browser.removeTab !== "function") {
    throw new Error("BrowserTestUtils.removeTab requires a removable tab");
  }

  Reflect.apply(browser.removeTab, browser, [
    tab,
    isRecord(options) ? options : undefined,
  ]);
  await waitForCondition(
    () => !tabListIncludes(tabList(browser), tab, globals),
    "BrowserTestUtils expected tab removal",
  );
}

async function switchTabCompat(
  gBrowserValue: unknown,
  tab: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  const gBrowser = waiveXrays(gBrowserValue, globals);
  if (!isRecord(gBrowser)) {
    throw new Error("BrowserTestUtils.switchTab requires a gBrowser");
  }
  gBrowser.selectedTab = tab;
  await waitForTabSwitch(gBrowser, tab, globals);
  return tab;
}

function createTestUtils(): Record<string, unknown> {
  return {
    async waitForCondition(
      condition: ConditionFunction,
      message?: string,
      interval?: number,
      maxTries?: number,
    ) {
      return await waitForMozillaCondition(
        condition,
        message,
        interval,
        maxTries,
      );
    },
  };
}

function createBrowserTestUtils(
  globals: Record<string, unknown>,
): Record<string, unknown> {
  return {
    addTab(gBrowser: unknown, urlOrOptions?: unknown, options?: unknown) {
      return addTabCompat(gBrowser, urlOrOptions, options, globals);
    },
    async browserLoaded(
      browser: unknown,
      includeSubFrames?: unknown,
      wanted?: unknown,
    ) {
      return await browserLoadedCompat(
        browser,
        includeSubFrames,
        wanted,
        globals,
      );
    },
    async browserStopped(browser: unknown) {
      return await browserStoppedCompat(browser, globals);
    },
    async waitForCondition(
      condition: ConditionFunction,
      message?: string,
      interval?: number,
      maxTries?: number,
    ) {
      return await waitForMozillaCondition(
        condition,
        message,
        interval,
        maxTries,
      );
    },
    async waitForEvent(
      target: unknown,
      eventName: string,
      capture?: unknown,
      checkFn?: unknown,
      wantsUntrusted?: unknown,
    ) {
      return await waitForEventCompat(
        target,
        eventName,
        capture,
        checkFn,
        wantsUntrusted,
        globals,
      );
    },
    async waitForMutationCondition(
      target: unknown,
      options: unknown,
      checkFn: unknown,
    ) {
      return await waitForMutationConditionCompat(
        target,
        options,
        checkFn,
        globals,
      );
    },
    async waitForNotificationInNotificationBox(
      notificationBox: unknown,
      notificationValue: unknown,
    ) {
      return await waitForNotificationInNotificationBoxCompat(
        notificationBox,
        notificationValue,
        globals,
      );
    },
    async openNewForegroundTab(
      gBrowser: unknown,
      opening?: unknown,
      waitForLoad?: unknown,
    ) {
      return await openNewForegroundTabCompat(
        gBrowser,
        opening,
        waitForLoad,
        globals,
      );
    },
    async removeTab(tab: unknown, options?: unknown) {
      await removeTabCompat(tab, options, globals);
    },
    async switchTab(gBrowser: unknown, tab: unknown) {
      return await switchTabCompat(gBrowser, tab, globals);
    },
    async withNewTab(opening: unknown, task: unknown) {
      const gBrowser = mostRecentBrowserWindow(globals).gBrowser;
      const tab = await openNewForegroundTabCompat(
        gBrowser,
        opening,
        true,
        globals,
      );
      const browser = browserForTab(
        waiveXrays(gBrowser, globals) as Record<string, unknown>,
        tab,
        globals,
      );
      try {
        if (typeof task === "function") {
          return await task(browser);
        }
        return undefined;
      } finally {
        await removeTabCompat(tab, undefined, globals);
      }
    },
  };
}

function deepEqual(actual: unknown, expected: unknown): boolean {
  if (Object.is(actual, expected)) {
    return true;
  }
  if (!isRecord(actual) || !isRecord(expected)) {
    return false;
  }
  const actualKeys = Object.keys(actual);
  const expectedKeys = Object.keys(expected);
  if (actualKeys.length !== expectedKeys.length) {
    return false;
  }
  for (const key of actualKeys) {
    if (
      !Object.hasOwn(expected, key) || !deepEqual(actual[key], expected[key])
    ) {
      return false;
    }
  }
  return true;
}

function createAssert(): Record<string, unknown> {
  return {
    deepEqual(actual: unknown, expected: unknown, message = "deepEqual"): void {
      if (!deepEqual(actual, expected)) {
        throw new Error(
          `${message} (expected: ${formatValue(expected)}, actual: ${
            formatValue(actual)
          })`,
        );
      }
    },
    notDeepEqual(
      actual: unknown,
      expected: unknown,
      message = "notDeepEqual",
    ): void {
      if (deepEqual(actual, expected)) {
        throw new Error(
          `${message} (did not expect: ${formatValue(expected)})`,
        );
      }
    },
    equal(actual: unknown, expected: unknown, message = "equal"): void {
      if (!Object.is(actual, expected)) {
        throw new Error(
          `${message} (expected: ${formatValue(expected)}, actual: ${
            formatValue(actual)
          })`,
        );
      }
    },
    notEqual(
      actual: unknown,
      unexpected: unknown,
      message = "notEqual",
    ): void {
      if (Object.is(actual, unexpected)) {
        throw new Error(
          `${message} (unexpected: ${formatValue(unexpected)})`,
        );
      }
    },
    notStrictEqual(
      actual: unknown,
      unexpected: unknown,
      message = "notStrictEqual",
    ): void {
      if (Object.is(actual, unexpected)) {
        throw new Error(
          `${message} (unexpected: ${formatValue(unexpected)})`,
        );
      }
    },
    ok(condition: unknown, message = "ok"): void {
      if (!condition) {
        throw new Error(message);
      }
    },
    strictEqual(
      actual: unknown,
      expected: unknown,
      message = "strictEqual",
    ): void {
      if (!Object.is(actual, expected)) {
        throw new Error(
          `${message} (expected: ${formatValue(expected)}, actual: ${
            formatValue(actual)
          })`,
        );
      }
    },
  };
}

function createGBrowserCompat(
  rawGBrowser: unknown,
  globals: Record<string, unknown>,
): unknown {
  const waivedGBrowser = waiveXrays(rawGBrowser, globals);
  if (!isRecord(waivedGBrowser)) {
    return waivedGBrowser;
  }

  return new Proxy(waivedGBrowser, {
    get(target, property, receiver) {
      const value = Reflect.get(target, property, receiver);

      if (property === "tabs" && isIterable(value)) {
        return Array.from(value, (tab) => waiveXrays(tab, globals));
      }

      if (typeof value === "function") {
        return (...args: unknown[]) =>
          waiveXrays(Reflect.apply(value, target, args), globals);
      }

      return waiveXrays(value, globals);
    },
    set(target, property, value, receiver) {
      return Reflect.set(target, property, value, receiver);
    },
  });
}

export class MozillaTaskContext {
  readonly #file: string;
  readonly #tasks: RegisteredTask[] = [];
  readonly #cleanups: CleanupFunction[] = [];
  readonly #previous = new Map<GlobalName, PreviousGlobal>();
  #installed = false;

  constructor(file: string) {
    this.#file = file;
  }

  get taskCount(): number {
    return this.#tasks.length;
  }

  install(): void {
    if (this.#installed) {
      return;
    }

    const globals = globalThis as Record<string, unknown>;
    for (const name of GLOBAL_NAMES) {
      this.#previous.set(name, {
        existed: objectHasOwn.call(globalThis, name),
        value: globals[name],
      });
    }

    if (usesMozillaBorrowedStyle(this.#file)) {
      const browserWindow = mostRecentBrowserWindow(globals);
      if (browserWindow.gBrowser) {
        globals.gBrowser = createGBrowserCompat(
          browserWindow.gBrowser,
          globals,
        );
      }
      if (browserWindow.gBrowserInit) {
        globals.gBrowserInit = waiveXrays(browserWindow.gBrowserInit, globals);
      }
    }

    globals.add_task = (fn: TaskFunction): void => {
      if (typeof fn !== "function") {
        throw new Error("add_task expects a function");
      }
      this.#tasks.push({
        name: taskName(fn, this.#tasks.length),
        fn,
      });
    };

    globals.registerCleanupFunction = (fn: CleanupFunction): void => {
      if (typeof fn !== "function") {
        throw new Error("registerCleanupFunction expects a function");
      }
      this.#cleanups.push(fn);
    };

    globals.ok = (condition: unknown, message = "ok"): void => {
      if (!condition) {
        throw new Error(message);
      }
    };

    globals.is = (
      actual: unknown,
      expected: unknown,
      message = "is",
    ): void => {
      if (!Object.is(actual, expected)) {
        throw new Error(
          `${message} (expected: ${formatValue(expected)}, actual: ${
            formatValue(actual)
          })`,
        );
      }
    };

    globals.isnot = (
      actual: unknown,
      unexpected: unknown,
      message = "isnot",
    ): void => {
      if (Object.is(actual, unexpected)) {
        throw new Error(
          `${message} (unexpected: ${formatValue(unexpected)})`,
        );
      }
    };

    globals.info = (message: unknown): void => {
      console.log(`[nora@test] ${String(message)}`);
    };

    globals.todo = (condition: unknown, message = "todo"): void => {
      const state = condition ? "unexpected pass" : "todo";
      console.log(`[nora@test] ${state}: ${message}`);
    };

    globals.Assert = createAssert();
    globals.TestUtils = createTestUtils();
    globals.BrowserTestUtils = createBrowserTestUtils(globals);
    globals.makeURI = (uri: string): unknown => {
      const services = servicesFromGlobals(globals);
      if (!services.io) {
        throw new Error("makeURI requires Services.io");
      }
      return services.io.newURI(uri);
    };

    this.#installed = true;
  }

  restore(): void {
    if (!this.#installed) {
      return;
    }

    const globals = globalThis as Record<string, unknown>;
    for (const name of GLOBAL_NAMES) {
      const previous = this.#previous.get(name);
      if (!previous) {
        continue;
      }

      if (previous.existed) {
        globals[name] = previous.value;
      } else {
        delete globals[name];
      }
    }

    this.#installed = false;
  }

  async runTasks(): Promise<void> {
    const failures: string[] = [];

    for (const task of this.#tasks) {
      try {
        await task.fn();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`${task.name}: ${message}`);
      }
    }

    const cleanupFailures = await this.#runCleanups();
    failures.push(...cleanupFailures);

    if (failures.length > 0) {
      throw new Error(
        `${this.#file} Mozilla task failures: ${failures.join(" | ")}`,
      );
    }
  }

  async cleanupAfterImportOnly(): Promise<void> {
    const cleanupFailures = await this.#runCleanups();
    if (cleanupFailures.length > 0) {
      throw new Error(
        `${this.#file} cleanup failures: ${cleanupFailures.join(" | ")}`,
      );
    }
  }

  async #runCleanups(): Promise<string[]> {
    const failures: string[] = [];
    let ordinal = 1;

    while (this.#cleanups.length > 0) {
      const cleanup = this.#cleanups.pop();
      if (!cleanup) {
        continue;
      }
      try {
        await cleanup();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`cleanup ${ordinal}: ${message}`);
      }
      ordinal++;
    }

    return failures;
  }
}
