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
  | "gBrowserInit"
  | "BrowserCommands"
  | "gURLBar";

type PreviousGlobal = {
  existed: boolean;
  value: unknown;
};

type RegisteredTask = {
  name: string;
  fn: TaskFunction;
};

type TrackedTabRemoval = {
  tab: unknown;
  gBrowser: Record<string, unknown>;
};

export interface MozillaTaskResult {
  index: number;
  name: string;
  ok: boolean;
  durationMs: number;
  error?: string;
}

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
  "BrowserCommands",
  "gURLBar",
];

const objectHasOwn = Object.prototype.hasOwnProperty;
const urlbarFocusedTabs: unknown[] = [];

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
  prefs?: {
    getBoolPref: (name: string, fallback?: boolean) => boolean;
  };
  scriptSecurityManager?: {
    getSystemPrincipal: () => unknown;
  };
  wm?: WindowMediatorLike;
};

type BrowserChromeWindowLike = {
  closed?: boolean;
  BrowserCommands?: unknown;
  gBrowser?: unknown;
  gBrowserInit?: unknown;
  gURLBar?: unknown;
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
  const maybeRecentWindow = services.wm?.getMostRecentBrowserWindow?.() ??
    services.wm?.getMostRecentWindow("navigator:browser");
  if (isHealthyBrowserWindow(maybeRecentWindow, globals)) {
    return maybeRecentWindow as BrowserChromeWindowLike;
  }

  const enumerator = services.wm?.getEnumerator?.("navigator:browser");
  let fallbackWindow: BrowserChromeWindowLike | undefined;
  if (enumerator) {
    while (enumerator.hasMoreElements()) {
      const maybeWindow = enumerator.getNext();
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

function healthyGBrowserForMutation(
  globals: Record<string, unknown>,
): Record<string, unknown> | undefined {
  const globalGBrowser = globals.gBrowser;
  if (isHealthyGBrowser(globalGBrowser, globals) && isRecord(globalGBrowser)) {
    return globalGBrowser;
  }

  const browserWindow = mostRecentBrowserWindow(globals);
  const windowGBrowser = browserWindow.gBrowser;
  return isHealthyGBrowser(windowGBrowser, globals) && isRecord(windowGBrowser)
    ? windowGBrowser
    : undefined;
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

function trackTabRemoval(
  trackedRemovals: TrackedTabRemoval[],
  tab: unknown,
  gBrowser: Record<string, unknown>,
): void {
  if (
    trackedRemovals.some((tracked) =>
      Object.is(tracked.tab, tab) && Object.is(tracked.gBrowser, gBrowser)
    )
  ) {
    return;
  }
  trackedRemovals.push({ tab, gBrowser });
}

async function drainTrackedTabRemovals(
  trackedRemovals: TrackedTabRemoval[],
): Promise<string[]> {
  const failures: string[] = [];
  const pending = trackedRemovals.splice(0);
  for (const [index, tracked] of pending.entries()) {
    try {
      await waitForCondition(
        () => {
          const absentFromBrowser = !tabList(tracked.gBrowser).some((tab) =>
            Object.is(tab, tracked.tab)
          );
          const tabRecord = isRecord(tracked.tab) ? tracked.tab : undefined;
          const disconnected = tabRecord &&
              typeof tabRecord.isConnected === "boolean"
            ? tabRecord.isConnected === false
            : true;
          return absentFromBrowser && disconnected;
        },
        "BrowserTestUtils expected complete tab removal",
        10000,
      );
    } catch (error) {
      failures.push(
        `tab removal ${index + 1}: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  }
  return failures;
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

function gURLBarRecord(
  globals: Record<string, unknown>,
): Record<string, unknown> | undefined {
  const globalURLBar = waiveXrays(globals.gURLBar, globals);
  if (isRecord(globalURLBar)) {
    return globalURLBar;
  }

  const browserWindow = mostRecentBrowserWindow(globals);
  const windowURLBar = waiveXrays(browserWindow.gURLBar, globals);
  return isRecord(windowURLBar) ? windowURLBar : undefined;
}

function rememberUrlbarFocusedTab(
  tab: unknown,
  globals: Record<string, unknown>,
): void {
  if (!tabListIncludes(urlbarFocusedTabs, tab, globals)) {
    urlbarFocusedTabs.push(waiveXrays(tab, globals));
  }
}

function forgetUrlbarFocusedTab(
  tab: unknown,
  globals: Record<string, unknown>,
): void {
  const index = urlbarFocusedTabs.findIndex((candidate) =>
    sameTab(candidate, tab, globals)
  );
  if (index >= 0) {
    urlbarFocusedTabs.splice(index, 1);
  }
}

function captureUrlbarFocusForSelectedTab(
  gBrowserValue: unknown,
  globals: Record<string, unknown>,
): void {
  const gBrowser = waiveXrays(gBrowserValue, globals);
  if (!isRecord(gBrowser) || !gBrowser.selectedTab) {
    return;
  }

  const urlbar = gURLBarRecord(globals);
  if (!urlbar || !("focused" in urlbar)) {
    return;
  }

  if (urlbar.focused === true) {
    rememberUrlbarFocusedTab(gBrowser.selectedTab, globals);
  } else {
    forgetUrlbarFocusedTab(gBrowser.selectedTab, globals);
  }
}

async function waitForRememberedUrlbarFocus(
  tab: unknown,
  globals: Record<string, unknown>,
): Promise<void> {
  if (!tabListIncludes(urlbarFocusedTabs, tab, globals)) {
    return;
  }

  const urlbar = gURLBarRecord(globals);
  if (!urlbar) {
    return;
  }

  try {
    await waitForCondition(
      () => urlbar.focused === true,
      "BrowserTestUtils expected urlbar focus to be restored",
      5000,
    );
  } catch {
    // Preserve the upstream assertion failure when focus is never restored.
  }
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

type TabSwitchWaitHandle = {
  promise: Promise<void>;
  cancel: () => void;
};

function armTabSwitchWait(
  gBrowser: Record<string, unknown>,
  globals: Record<string, unknown>,
  timeoutMs = 5000,
): TabSwitchWaitHandle {
  const target = eventTargetRecord(gBrowser, globals);
  if (!target) {
    throw new Error("BrowserTestUtils.switchTab requires a tabbrowser target");
  }

  const addEventListener = callableMethod(target, "addEventListener");
  const removeEventListener = callableMethod(target, "removeEventListener");
  if (!addEventListener || !removeEventListener) {
    throw new Error("BrowserTestUtils.switchTab requires event listeners");
  }

  const services = servicesFromGlobals(globals);
  const ownerDocument = waiveXrays(target.ownerDocument, globals);
  let waitForDelayedSwitch = false;
  try {
    waitForDelayedSwitch = services.prefs?.getBoolPref(
      "test.wait300msAfterTabSwitch",
      false,
    ) === true;
  } catch {
    // Match the upstream false fallback when the pref service is unavailable.
  }
  const eventName = waitForDelayedSwitch ||
      (isRecord(ownerDocument) && ownerDocument.hidden === true)
    ? "TabSwitchDone"
    : "TabSwitched";

  let resolveWait: () => void = () => {};
  let rejectWait: (reason: unknown) => void = () => {};
  let settled = false;
  let listenerRegistered = false;
  let timeoutId: ReturnType<typeof setTimeout> | undefined;
  const promise = new Promise<void>((resolve, reject) => {
    resolveWait = resolve;
    rejectWait = reject;
  });

  const cleanup = (): void => {
    if (timeoutId !== undefined) {
      clearTimeout(timeoutId);
      timeoutId = undefined;
    }
    if (listenerRegistered) {
      listenerRegistered = false;
      try {
        removeEventListener.call(target, eventName, onEvent, false);
      } catch {
        // Event targets may reject late removal during shutdown.
      }
    }
  };

  const onEvent = (): void => {
    if (settled) {
      return;
    }
    settled = true;
    cleanup();
    setTimeout(resolveWait, 0);
  };

  timeoutId = setTimeout(() => {
    if (settled) {
      return;
    }
    settled = true;
    cleanup();
    rejectWait(new Error(`Timed out waiting for ${eventName}`));
  }, timeoutMs);

  try {
    listenerRegistered = true;
    addEventListener.call(target, eventName, onEvent, false);
  } catch (error) {
    settled = true;
    cleanup();
    resolveWait();
    throw error;
  }

  return {
    promise,
    cancel(): void {
      if (settled) {
        return;
      }
      settled = true;
      cleanup();
      resolveWait();
    },
  };
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
  const options: Record<string, unknown> = { ...extraOptions };
  if (!("triggeringPrincipal" in options)) {
    const principal = services.scriptSecurityManager?.getSystemPrincipal();
    if (principal) {
      options.triggeringPrincipal = principal;
    }
  }
  return options;
}

function addTabCompat(
  gBrowserValue: unknown,
  urlOrOptions: unknown,
  maybeOptions: unknown,
  globals: Record<string, unknown>,
): unknown {
  const gBrowser = gBrowserValue;
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
  const returnedTab = gBrowser.addTab(
    url,
    makeLoadOptions(globals, optionsInput),
  );
  if (returnedTab) {
    return returnedTab;
  }

  const newTab = tabList(gBrowser).find((tab) =>
    !tabListIncludes(previousTabs, tab, globals)
  );
  if (newTab) {
    return newTab;
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
  const gBrowser = gBrowserValue;
  if (!isRecord(gBrowser)) {
    throw new Error(
      "BrowserTestUtils.openNewForegroundTab requires a gBrowser",
    );
  }

  const previousTabs = tabList(gBrowser);
  let tab: unknown;
  const switchPromise = switchTabCompat(
    gBrowser,
    () => {
      if (typeof opening === "function") {
        opening();
        tab = gBrowser.selectedTab;
        if (tabListIncludes(previousTabs, tab, globals)) {
          throw new Error(
            "BrowserTestUtils expected the callback to select a new tab",
          );
        }
      } else {
        tab = addTabCompat(gBrowser, opening, undefined, globals);
        gBrowser.selectedTab = tab;
      }

      if (!tab) {
        throw new Error("BrowserTestUtils expected a new tab");
      }
    },
    globals,
  );

  if (!tab) {
    void switchPromise.catch(() => undefined);
    throw new Error("BrowserTestUtils expected a new tab");
  }

  const browser = browserForTab(gBrowser, tab, globals);
  if (!isRecord(browser)) {
    void switchPromise.catch(() => undefined);
    throw new Error("BrowserTestUtils expected the new tab browser");
  }
  const requestedUrl = typeof opening === "string"
    ? opening
    : opening === undefined
    ? "about:blank"
    : undefined;
  const loadPromise = waitForLoad !== false && requestedUrl
    ? browserLoadedCompat(browser, false, requestedUrl, globals)
    : undefined;

  await switchPromise;
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

async function resetBrowserTabsForMozillaTask(
  globals: Record<string, unknown>,
  trackedRemovals: TrackedTabRemoval[],
): Promise<void> {
  const gBrowser = healthyGBrowserForMutation(globals);
  if (!gBrowser) {
    return;
  }

  const existingTabs = tabList(gBrowser);
  if (existingTabs.length === 0) {
    return;
  }

  urlbarFocusedTabs.length = 0;
  let keeper: unknown;
  await switchTabCompat(
    gBrowser,
    () => {
      keeper = addTabCompat(gBrowser, "about:blank", undefined, globals);
      gBrowser.selectedTab = keeper;
      return keeper;
    },
    globals,
  );

  const keeperBrowser = browserForTab(gBrowser, keeper, globals);
  await browserLoadedCompat(
    keeperBrowser,
    false,
    "about:blank",
    globals,
  ).catch(() => undefined);

  for (const tab of existingTabs) {
    if (
      sameTab(tab, keeper, globals) ||
      !tabListIncludes(tabList(gBrowser), tab, globals)
    ) {
      continue;
    }
    removeTabCompat(
      tab,
      {
        animate: false,
        skipPermitUnload: true,
      },
      globals,
      trackedRemovals,
    );
  }

  const removalFailures = await drainTrackedTabRemovals(
    trackedRemovals,
  );
  if (removalFailures.length > 0) {
    throw new Error(removalFailures.join(" | "));
  }

  await waitForCondition(
    () =>
      tabList(gBrowser).length === 1 &&
      sameTab(gBrowser.selectedTab, keeper, globals),
    "Mozilla task expected a clean single-tab browser window",
    10000,
  );
}

function browserWindowFromTab(
  tab: unknown,
): Record<string, unknown> | undefined {
  const tabRecord = tab;
  if (isRecord(tabRecord)) {
    const browserWindow = tabRecord.documentGlobal || tabRecord.ownerGlobal;
    if (isRecord(browserWindow)) {
      return browserWindow;
    }
  }
  return undefined;
}

function removeTabCompat(
  tab: unknown,
  options: unknown,
  globals: Record<string, unknown>,
  trackedRemovals: TrackedTabRemoval[],
): void {
  const browserWindow = browserWindowFromTab(tab);
  const gBrowser = browserWindow?.gBrowser;
  if (!isRecord(gBrowser) || typeof gBrowser.removeTab !== "function") {
    throw new Error("BrowserTestUtils.removeTab requires a removable tab");
  }

  gBrowser.removeTab(
    tab,
    isRecord(options) ? options : {},
  );
  trackTabRemoval(trackedRemovals, tab, gBrowser);
  forgetUrlbarFocusedTab(tab, globals);
}

function switchTabCompat(
  gBrowserValue: unknown,
  switching: unknown,
  globals: Record<string, unknown>,
): Promise<unknown> {
  const gBrowser = gBrowserValue;
  if (!isRecord(gBrowser)) {
    throw new Error("BrowserTestUtils.switchTab requires a gBrowser");
  }
  captureUrlbarFocusForSelectedTab(gBrowser, globals);
  const previouslySelectedTab = gBrowser.selectedTab;
  if (
    typeof switching !== "function" &&
    sameTab(previouslySelectedTab, switching, globals) &&
    isRecord(browserForTab(gBrowser, switching, globals))
  ) {
    return Promise.resolve(switching);
  }

  const switchWait = armTabSwitchWait(gBrowser, globals);
  let tab: unknown;
  try {
    if (typeof switching === "function") {
      switching();
      tab = gBrowser.selectedTab;
    } else {
      tab = switching;
      gBrowser.selectedTab = tab;
    }

    if (!tab) {
      throw new Error("BrowserTestUtils.switchTab expected a selected tab");
    }
    if (
      sameTab(previouslySelectedTab, tab, globals) &&
      isRecord(browserForTab(gBrowser, tab, globals))
    ) {
      switchWait.cancel();
    }
  } catch (error) {
    switchWait.cancel();
    throw error;
  }

  return (async () => {
    await switchWait.promise;
    await waitForCondition(
      () =>
        sameTab(gBrowser.selectedTab, tab, globals) &&
        isRecord(browserForTab(gBrowser, tab, globals)),
      "BrowserTestUtils expected tab switch",
    );
    await waitForRememberedUrlbarFocus(tab, globals);
    captureUrlbarFocusForSelectedTab(gBrowser, globals);
    return tab;
  })();
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
  trackedRemovals: TrackedTabRemoval[],
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
    removeTab(tab: unknown, options?: unknown): void {
      removeTabCompat(tab, options, globals, trackedRemovals);
    },
    async switchTab(gBrowser: unknown, tab: unknown) {
      return await switchTabCompat(gBrowser, tab, globals);
    },
    async withNewTab(opening: unknown, task: unknown) {
      const gBrowser = healthyGBrowserForMutation(globals);
      if (!gBrowser) {
        throw new Error("BrowserTestUtils.withNewTab requires a gBrowser");
      }
      const tab = await openNewForegroundTabCompat(
        gBrowser,
        opening,
        true,
        globals,
      );
      const browser = browserForTab(
        gBrowser,
        tab,
        globals,
      );
      let result: unknown;
      const failures: string[] = [];
      try {
        if (typeof task === "function") {
          result = await task(browser);
        }
      } catch (error) {
        failures.push(error instanceof Error ? error.message : String(error));
      }

      try {
        removeTabCompat(tab, undefined, globals, trackedRemovals);
        const removalFailures = await drainTrackedTabRemovals(
          trackedRemovals,
        );
        failures.push(...removalFailures);
      } catch (error) {
        failures.push(error instanceof Error ? error.message : String(error));
      }

      if (failures.length > 0) {
        throw new Error(failures.join(" | "));
      }
      return result;
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

export class MozillaTaskContext {
  readonly #file: string;
  readonly #tasks: RegisteredTask[] = [];
  readonly #taskResults: MozillaTaskResult[] = [];
  readonly #cleanups: CleanupFunction[] = [];
  readonly #trackedTabRemovals: TrackedTabRemoval[] = [];
  readonly #previous = new Map<GlobalName, PreviousGlobal>();
  #installed = false;

  constructor(file: string) {
    this.#file = file;
  }

  get taskCount(): number {
    return this.#tasks.length;
  }

  get taskResults(): readonly MozillaTaskResult[] {
    return this.#taskResults;
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
      if (
        !isHealthyGBrowser(globals.gBrowser, globals) &&
        browserWindow.gBrowser
      ) {
        globals.gBrowser = browserWindow.gBrowser;
      }
      if (
        !isRecord(waiveXrays(globals.gBrowserInit, globals)) &&
        browserWindow.gBrowserInit
      ) {
        globals.gBrowserInit = browserWindow.gBrowserInit;
      }
      if (
        !isRecord(waiveXrays(globals.BrowserCommands, globals)) &&
        browserWindow.BrowserCommands
      ) {
        globals.BrowserCommands = browserWindow.BrowserCommands;
      }
      if (
        !isRecord(waiveXrays(globals.gURLBar, globals)) &&
        browserWindow.gURLBar
      ) {
        globals.gURLBar = browserWindow.gURLBar;
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
    globals.BrowserTestUtils = createBrowserTestUtils(
      globals,
      this.#trackedTabRemovals,
    );
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

  async runTasks(): Promise<readonly MozillaTaskResult[]> {
    const failures: string[] = [];
    this.#taskResults.length = 0;
    let setupOk = true;
    if (usesMozillaBorrowedStyle(this.#file)) {
      try {
        await resetBrowserTabsForMozillaTask(
          globalThis as Record<string, unknown>,
          this.#trackedTabRemovals,
        );
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        failures.push(`browser setup: ${message}`);
        setupOk = false;
      }
    }

    if (setupOk) {
      for (const [taskIndex, task] of this.#tasks.entries()) {
        const startedAtMs = Date.now();
        (globalThis as Record<string, unknown>).__NORA_TEST_PROGRESS__ = {
          moduleName: this.#file,
          testName: task.name,
          status: "running",
          index: taskIndex + 1,
          total: this.#tasks.length,
          startedAtMs,
        };
        let taskFailure: string | undefined;
        try {
          await task.fn();
        } catch (error) {
          taskFailure = error instanceof Error ? error.message : String(error);
        } finally {
          const removalFailures = await drainTrackedTabRemovals(
            this.#trackedTabRemovals,
          );
          if (removalFailures.length > 0) {
            taskFailure = [
              taskFailure,
              ...removalFailures.map((failure) => `post-task ${failure}`),
            ].filter((failure): failure is string => Boolean(failure)).join(
              " | ",
            );
          }

          this.#taskResults.push({
            index: taskIndex + 1,
            name: task.name,
            ok: taskFailure === undefined,
            durationMs: Math.max(0, Date.now() - startedAtMs),
            error: taskFailure,
          });
          if (taskFailure !== undefined) {
            failures.push(`${task.name}: ${taskFailure}`);
          }

          (globalThis as Record<string, unknown>).__NORA_TEST_PROGRESS__ = {
            moduleName: this.#file,
            testName: task.name,
            status: "done",
            index: taskIndex + 1,
            total: this.#tasks.length,
            startedAtMs,
          };
        }
      }
    }

    const cleanupFailures = await this.#runCleanups();
    failures.push(...cleanupFailures);

    if (failures.length > 0) {
      throw new Error(
        `${this.#file} Mozilla task failures: ${failures.join(" | ")}`,
      );
    }

    return this.taskResults;
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

    const removalFailures = await drainTrackedTabRemovals(
      this.#trackedTabRemovals,
    );
    failures.push(
      ...removalFailures.map((failure) => `post-cleanup ${failure}`),
    );

    return failures;
  }
}
