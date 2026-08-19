// SPDX-License-Identifier: MPL-2.0

/**
 * Browser-side test entry point.
 *
 * Loaded by chrome_root.ts when MODE === "test".
 * Discovers all @colocated-env browser test files via import.meta.glob,
 * runs them sequentially, and writes structured results to
 * globalThis.__TEST_RESULTS__ and the Firefox pref "nora.tests.state".
 * The host-side collector reads results from the profile's prefs.js file.
 */

import {
  MozillaTaskContext,
  type MozillaTaskResult,
} from "./mochitest_compat.ts";

type DownloadedFirefoxPref =
  | { name: string; kind: "bool"; value: boolean }
  | { name: string; kind: "int"; value: number }
  | { name: string; kind: "string"; value: string };

interface DownloadedFirefoxTestMarker {
  schemaVersion: 1;
  upstreamPath: string;
  manifestPath: string;
  expectedTaskCount: number;
  prefs: DownloadedFirefoxPref[];
  headPolicy: "harness-replaced";
  supportPolicy: "locked-not-loaded";
  load: LazyModule;
}

export interface TestResult {
  file: string;
  ok: boolean;
  durationMs: number;
  mode: "import" | "runAllTests" | "mozillaTasks";
  source?: "downloaded-firefox";
  upstreamPath?: string;
  manifestPath?: string;
  tasks?: MozillaTaskResult[];
  error?: string;
  timedOut?: boolean;
}

interface TestState {
  status: "running" | "done" | "error";
  results: TestResult[];
  discoveredFiles: string[];
  runId?: string;
  aborted?: boolean;
  abortReason?: string;
  error?: string;
}

const TEST_STATE_PREF = "nora.tests.state";
const TEST_FILTER_PREF = "nora.tests.filter";
const TEST_FILTER_COUNT_PREF = "nora.tests.filter.count";
const TEST_FILTER_ITEM_PREF_PREFIX = "nora.tests.filter.";
const TEST_RUN_ID_PREF = "nora.tests.run_id";
const TEST_CONTROL_FILE = "nora-tests-control.json";
const TEST_CONTROL_SCHEMA_VERSION = 1;
const DOWNLOADED_FIREFOX_TEST_EXPORT = "__NORA_DOWNLOADED_FIREFOX_TEST__";

declare global {
  var __TEST_RESULTS__: TestState | undefined;
  var __NORA_TEST_PROGRESS__:
    | {
      moduleName: string;
      testName: string;
      status: "running" | "passed" | "failed" | "done";
      index: number;
      total: number;
      startedAtMs: number;
    }
    | undefined;
}

function setSharedStatePref(state: TestState): void {
  const payload = JSON.stringify(state);

  try {
    const maybeServices = (globalThis as Record<string, unknown>).Services as
      | {
        prefs?: {
          setStringPref: (name: string, value: string) => void;
          savePrefFile?: (prefFile: string | null) => void;
        };
      }
      | undefined;
    if (maybeServices?.prefs) {
      maybeServices.prefs.setStringPref(TEST_STATE_PREF, payload);
      maybeServices.prefs.savePrefFile?.(null);
      return;
    }
  } catch {
    // try fallback path below
  }

  try {
    const chromeUtils = (globalThis as Record<string, unknown>).ChromeUtils as
      | {
        importESModule: (specifier: string) => {
          Services?: {
            prefs?: {
              setStringPref: (name: string, value: string) => void;
              savePrefFile?: (prefFile: string | null) => void;
            };
          };
        };
      }
      | undefined;

    const servicesFromModule = chromeUtils?.importESModule(
      "resource://gre/modules/Services.sys.mjs",
    ).Services;
    servicesFromModule?.prefs?.setStringPref(TEST_STATE_PREF, payload);
    // Force immediate flush to disk so the host-side runner can read
    // results from prefs.js even if the browser shuts down soon after.
    // null = flush to the default prefs.js file immediately.
    servicesFromModule?.prefs?.savePrefFile?.(null);
  } catch {
    // Keep browser-side tests running even if prefs are unavailable.
  }
}

function publishState(state: TestState): void {
  globalThis.__TEST_RESULTS__ = state;
  setSharedStatePref(state);
}

// Keep this below host-side collection timeout defaults while giving slow
// test environments enough room.
const PER_TEST_TIMEOUT_MS = 120_000;

export type LazyModule = () => Promise<unknown>;

class TimeoutError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "TimeoutError";
  }
}

function nowMs(): number {
  return globalThis.performance?.now() ?? Date.now();
}

function browserWindows(): Array<Record<string, unknown>> {
  try {
    const services = (globalThis as Record<string, unknown>).Services as
      | {
        wm?: {
          getEnumerator?: (windowType: string) => {
            hasMoreElements: () => boolean;
            getNext: () => unknown;
          };
        };
      }
      | undefined;
    const enumerator = services?.wm?.getEnumerator?.("navigator:browser");
    if (!enumerator) {
      return [];
    }

    const windows: Array<Record<string, unknown>> = [];
    while (enumerator.hasMoreElements()) {
      const value = enumerator.getNext();
      if (isRecord(value)) {
        windows.push(value);
      }
    }
    return windows;
  } catch {
    return [];
  }
}

async function restorePrimaryBrowserWindow(): Promise<void> {
  const primaryWindow = globalThis as Record<string, unknown>;
  for (const browserWindow of browserWindows()) {
    if (Object.is(browserWindow, primaryWindow) || browserWindow.closed) {
      continue;
    }
    const close = callableMethod(browserWindow, "close");
    try {
      close?.call(browserWindow);
    } catch {
      // Keep cleanup best-effort; the failing test result is more useful.
    }
  }

  const focus = callableMethod(primaryWindow, "focus");
  try {
    focus?.call(primaryWindow);
  } catch {
    // Focus may be unavailable in headless or shutdown-adjacent states.
  }

  const deadline = Date.now() + 1000;
  while (
    Date.now() < deadline &&
    browserWindows().some((browserWindow) =>
      !Object.is(browserWindow, primaryWindow) && !browserWindow.closed
    )
  ) {
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

function normalizeLoadedModule(value: unknown): Record<string, unknown> {
  if (value && typeof value === "object") {
    return value as Record<string, unknown>;
  }

  // Defensive normalization for unusual dynamic import return shapes.
  return {};
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object";
}

function readDownloadedFirefoxTestMarker(
  mod: Record<string, unknown>,
): DownloadedFirefoxTestMarker | undefined {
  const value = mod[DOWNLOADED_FIREFOX_TEST_EXPORT];
  if (value === undefined) {
    return undefined;
  }
  if (!isRecord(value)) {
    throw new Error(`${DOWNLOADED_FIREFOX_TEST_EXPORT} must be an object`);
  }

  const schemaVersion = value.schemaVersion;
  const upstreamPath = value.upstreamPath;
  const manifestPath = value.manifestPath;
  const expectedTaskCount = value.expectedTaskCount;
  const rawPrefs = value.prefs;
  const headPolicy = value.headPolicy;
  const supportPolicy = value.supportPolicy;
  const load = value.load;

  if (schemaVersion !== 1) {
    throw new Error("Downloaded Firefox test marker schemaVersion must be 1");
  }
  if (typeof upstreamPath !== "string" || upstreamPath.length === 0) {
    throw new Error("Downloaded Firefox test marker needs upstreamPath");
  }
  if (typeof manifestPath !== "string" || manifestPath.length === 0) {
    throw new Error("Downloaded Firefox test marker needs manifestPath");
  }
  if (
    typeof expectedTaskCount !== "number" ||
    !Number.isSafeInteger(expectedTaskCount) || expectedTaskCount <= 0
  ) {
    throw new Error(
      "Downloaded Firefox test marker expectedTaskCount must be a positive integer",
    );
  }
  if (headPolicy !== "harness-replaced") {
    throw new Error(
      "Downloaded Firefox test marker headPolicy must be harness-replaced",
    );
  }
  if (supportPolicy !== "locked-not-loaded") {
    throw new Error(
      "Downloaded Firefox test marker supportPolicy must be locked-not-loaded",
    );
  }
  if (typeof load !== "function") {
    throw new Error(
      "Downloaded Firefox test marker needs a lazy load function",
    );
  }
  if (!Array.isArray(rawPrefs)) {
    throw new Error("Downloaded Firefox test marker prefs must be an array");
  }

  const seenPrefNames = new Set<string>();
  const prefs = rawPrefs.map((rawPref, index): DownloadedFirefoxPref => {
    if (!isRecord(rawPref)) {
      throw new Error(`Downloaded Firefox pref ${index} must be an object`);
    }
    const { name, kind, value: prefValue } = rawPref;
    if (typeof name !== "string" || name.length === 0) {
      throw new Error(`Downloaded Firefox pref ${index} needs a name`);
    }
    if (seenPrefNames.has(name)) {
      throw new Error(`Downloaded Firefox pref ${name} is duplicated`);
    }
    seenPrefNames.add(name);

    if (kind === "bool" && typeof prefValue === "boolean") {
      return { name, kind, value: prefValue };
    }
    if (
      kind === "int" && Number.isSafeInteger(prefValue) &&
      typeof prefValue === "number"
    ) {
      return { name, kind, value: prefValue };
    }
    if (kind === "string" && typeof prefValue === "string") {
      return { name, kind, value: prefValue };
    }
    throw new Error(
      `Downloaded Firefox pref ${name} has an invalid ${String(kind)} value`,
    );
  });

  return {
    schemaVersion,
    upstreamPath,
    manifestPath,
    expectedTaskCount,
    prefs,
    headPolicy,
    supportPolicy,
    load: load as LazyModule,
  };
}

interface PrefBranchLike {
  prefHasUserValue(name: string): boolean;
  getPrefType(name: string): number;
  getBoolPref(name: string): boolean;
  getIntPref(name: string): number;
  getStringPref(name: string): string;
  setBoolPref(name: string, value: boolean): void;
  setIntPref(name: string, value: number): void;
  setStringPref(name: string, value: string): void;
  clearUserPref(name: string): void;
}

type PrefSnapshot =
  | { name: string; hadUserValue: false }
  | {
    name: string;
    hadUserValue: true;
    value:
      | { kind: "bool"; value: boolean }
      | { kind: "int"; value: number }
      | { kind: "string"; value: string };
  };

const PREF_STRING = 32;
const PREF_INT = 64;
const PREF_BOOL = 128;

function browserPrefs(): PrefBranchLike {
  const globals = globalThis as Record<string, unknown>;
  const directServices = globals.Services;
  if (isRecord(directServices) && isRecord(directServices.prefs)) {
    return directServices.prefs as unknown as PrefBranchLike;
  }

  const chromeUtils = globals.ChromeUtils;
  const importESModule = isRecord(chromeUtils)
    ? chromeUtils.importESModule
    : undefined;
  if (typeof importESModule === "function") {
    const module = importESModule.call(
      chromeUtils,
      "resource://gre/modules/Services.sys.mjs",
    );
    if (isRecord(module) && isRecord(module.Services)) {
      const prefs = module.Services.prefs;
      if (isRecord(prefs)) {
        return prefs as unknown as PrefBranchLike;
      }
    }
  }

  throw new Error("Services.prefs is unavailable for downloaded Firefox test");
}

function snapshotPref(prefs: PrefBranchLike, name: string): PrefSnapshot {
  if (!prefs.prefHasUserValue(name)) {
    return { name, hadUserValue: false };
  }

  const prefType = prefs.getPrefType(name);
  if (prefType === PREF_BOOL) {
    return {
      name,
      hadUserValue: true,
      value: { kind: "bool", value: prefs.getBoolPref(name) },
    };
  }
  if (prefType === PREF_INT) {
    return {
      name,
      hadUserValue: true,
      value: { kind: "int", value: prefs.getIntPref(name) },
    };
  }
  if (prefType === PREF_STRING) {
    return {
      name,
      hadUserValue: true,
      value: { kind: "string", value: prefs.getStringPref(name) },
    };
  }
  throw new Error(`Cannot snapshot unsupported pref type ${prefType}: ${name}`);
}

function setTypedPref(
  prefs: PrefBranchLike,
  pref: DownloadedFirefoxPref,
): void {
  if (pref.kind === "bool") {
    prefs.setBoolPref(pref.name, pref.value);
  } else if (pref.kind === "int") {
    prefs.setIntPref(pref.name, pref.value);
  } else {
    prefs.setStringPref(pref.name, pref.value);
  }
}

function restorePrefs(
  prefs: PrefBranchLike,
  snapshots: readonly PrefSnapshot[],
): string[] {
  const failures: string[] = [];
  for (const snapshot of [...snapshots].reverse()) {
    try {
      if (!snapshot.hadUserValue) {
        prefs.clearUserPref(snapshot.name);
      } else {
        setTypedPref(prefs, { name: snapshot.name, ...snapshot.value });
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`restore pref ${snapshot.name}: ${message}`);
    }
  }
  return failures;
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

function normalizeBrowserTestPath(value: string): string {
  const normalized = value.replaceAll("\\", "/");

  if (normalized.startsWith("/link-features-chrome/")) {
    return normalized.replace(
      "/link-features-chrome/",
      "browser-features/chrome/",
    );
  }
  if (normalized.startsWith("link-features-chrome/")) {
    return normalized.replace(
      "link-features-chrome/",
      "browser-features/chrome/",
    );
  }
  if (normalized.startsWith("#features-chrome/")) {
    return normalized.replace(
      "#features-chrome/",
      "browser-features/chrome/",
    );
  }
  if (normalized.startsWith("#features-modules/")) {
    return normalized.replace(
      "#features-modules/",
      "browser-features/modules/",
    );
  }
  if (normalized.startsWith("#features-pages/")) {
    return normalized.replace(
      "#features-pages/",
      "browser-features/",
    );
  }
  if (normalized.startsWith("../../browser-features/")) {
    return normalized.replace("../../browser-features/", "browser-features/");
  }

  const marker = "/browser-features/";
  const markerIndex = normalized.indexOf(marker);
  if (markerIndex >= 0) {
    return normalized.slice(markerIndex + 1);
  }

  return normalized;
}

function setFromFilterEntries(entries: unknown): Set<string> | undefined {
  if (!Array.isArray(entries)) {
    return undefined;
  }
  return new Set(
    entries
      .filter((entry): entry is string => typeof entry === "string")
      .map(normalizeBrowserTestPath),
  );
}

interface BrowserTestControl {
  schemaVersion: typeof TEST_CONTROL_SCHEMA_VERSION;
  runId: string;
  expiresAtMs: number;
  filter: string[];
}

export interface ConsumedBrowserTestControl {
  runId?: string;
  requestedTestFiles: Set<string>;
}

interface TestControlPrefs {
  getStringPref(name: string, fallback?: string): string;
  getChildList?(prefix: string): string[];
  prefHasUserValue?(name: string): boolean;
  clearUserPref?(name: string): void;
  savePrefFile?(prefFile: string | null): void;
}

function parseBrowserTestControl(
  value: unknown,
  nowMs: number,
): BrowserTestControl | undefined {
  if (
    !isRecord(value) || value.schemaVersion !== TEST_CONTROL_SCHEMA_VERSION ||
    typeof value.runId !== "string" || value.runId.length === 0 ||
    typeof value.expiresAtMs !== "number" ||
    !Number.isSafeInteger(value.expiresAtMs) || value.expiresAtMs < nowMs ||
    !Array.isArray(value.filter) ||
    !value.filter.every((entry) => typeof entry === "string")
  ) {
    return undefined;
  }
  return {
    schemaVersion: TEST_CONTROL_SCHEMA_VERSION,
    runId: value.runId,
    expiresAtMs: value.expiresAtMs,
    filter: value.filter,
  };
}

function readFilterEntriesFromPrefs(
  prefs: TestControlPrefs,
): string[] | undefined {
  const countRaw = prefs.getStringPref(TEST_FILTER_COUNT_PREF, "");
  const count = Number(countRaw);
  if (!Number.isSafeInteger(count) || count < 0 || count > 10_000) {
    return undefined;
  }
  const entries: string[] = [];
  for (let index = 0; index < count; index++) {
    const entry = prefs.getStringPref(
      `${TEST_FILTER_ITEM_PREF_PREFIX}${index}`,
      "",
    );
    if (!entry) return undefined;
    entries.push(entry);
  }
  return entries;
}

function normalizedFilterEntries(entries: string[]): string[] {
  return entries.map(normalizeBrowserTestPath);
}

function filtersEqual(left: string[], right: string[]): boolean {
  const normalizedLeft = normalizedFilterEntries(left);
  const normalizedRight = normalizedFilterEntries(right);
  return normalizedLeft.length === normalizedRight.length &&
    normalizedLeft.every((entry, index) => entry === normalizedRight[index]);
}

function clearControlPrefs(prefs: TestControlPrefs): void {
  if (!prefs.clearUserPref) {
    throw new Error(
      "Firefox prefs service cannot clear one-shot test controls.",
    );
  }
  if (!prefs.savePrefFile) {
    throw new Error(
      "Firefox prefs service cannot persist one-shot test control cleanup.",
    );
  }
  const names = new Set<string>([
    TEST_FILTER_PREF,
    TEST_FILTER_COUNT_PREF,
    TEST_RUN_ID_PREF,
  ]);
  for (const name of prefs.getChildList?.(TEST_FILTER_ITEM_PREF_PREFIX) ?? []) {
    if (
      name === TEST_FILTER_COUNT_PREF ||
      new RegExp(
        `^${TEST_FILTER_ITEM_PREF_PREFIX.replaceAll(".", "\\.")}\\d+$`,
      ).test(name)
    ) {
      names.add(name);
    }
  }
  const count = Number(prefs.getStringPref(TEST_FILTER_COUNT_PREF, ""));
  if (Number.isSafeInteger(count) && count >= 0 && count <= 10_000) {
    for (let index = 0; index < count; index++) {
      names.add(`${TEST_FILTER_ITEM_PREF_PREFIX}${index}`);
    }
  }
  for (const name of names) {
    if (prefs.prefHasUserValue && !prefs.prefHasUserValue(name)) continue;
    try {
      prefs.clearUserPref(name);
    } catch {
      if (prefs.prefHasUserValue?.(name) !== false) {
        throw new Error(
          `Could not clear one-shot Firefox test pref ${name}.`,
        );
      }
    }
  }
  prefs.savePrefFile(null);
}

export async function consumeRequestedTestControl(
  nowMs = Date.now(),
): Promise<ConsumedBrowserTestControl> {
  const globals = globalThis as Record<string, unknown>;
  const services = globals.Services;
  const prefs = isRecord(services) && isRecord(services.prefs)
    ? services.prefs as unknown as TestControlPrefs
    : undefined;
  if (!prefs) {
    throw new Error("Firefox prefs service is unavailable for test control.");
  }

  const pathUtils = globals.PathUtils;
  const profilePath = isRecord(pathUtils) ? pathUtils.profileDir : undefined;
  const join = callableMethod(pathUtils, "join");
  if (
    typeof profilePath !== "string" || profilePath.length === 0 || !join
  ) {
    throw new Error(
      "Firefox PathUtils.profileDir and PathUtils.join are required for test control.",
    );
  }
  const ioUtils = globals.IOUtils;
  const readUTF8 = callableMethod(ioUtils, "readUTF8");
  const remove = callableMethod(ioUtils, "remove");
  const controlPath = join.call(pathUtils, profilePath, TEST_CONTROL_FILE);
  if (typeof controlPath !== "string" || controlPath.length === 0) {
    throw new Error(
      "Firefox PathUtils.join did not return a test control path.",
    );
  }

  let parsedControl: BrowserTestControl | undefined;
  try {
    if (readUTF8) {
      const raw = await readUTF8.call(ioUtils, controlPath);
      if (typeof raw === "string") {
        parsedControl = parseBrowserTestControl(JSON.parse(raw), nowMs);
      }
    }
  } catch {
    parsedControl = undefined;
  }

  let consumed: ConsumedBrowserTestControl = {
    requestedTestFiles: new Set(),
  };
  const errors: string[] = [];
  try {
    if (parsedControl) {
      const prefRunId = prefs.getStringPref(TEST_RUN_ID_PREF, "");
      const prefFilter = readFilterEntriesFromPrefs(prefs);
      let legacyFilter: unknown;
      try {
        legacyFilter = JSON.parse(prefs.getStringPref(TEST_FILTER_PREF, ""));
      } catch {
        legacyFilter = undefined;
      }
      if (
        prefRunId === parsedControl.runId && prefFilter &&
        filtersEqual(prefFilter, parsedControl.filter) &&
        Array.isArray(legacyFilter) &&
        legacyFilter.every((entry) => typeof entry === "string") &&
        filtersEqual(legacyFilter, parsedControl.filter)
      ) {
        consumed = {
          runId: parsedControl.runId,
          requestedTestFiles: setFromFilterEntries(parsedControl.filter) ??
            new Set(),
        };
      }
    }
  } catch (error) {
    errors.push(
      `could not validate browser test control: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }

  try {
    clearControlPrefs(prefs);
  } catch (error) {
    errors.push(
      `could not clear browser test control prefs: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }

  try {
    if (remove) {
      await remove.call(ioUtils, controlPath, { ignoreAbsent: true });
    } else {
      throw new Error("Firefox IO service cannot consume test control file.");
    }
  } catch (error) {
    errors.push(
      `could not remove browser test control file: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }

  if (errors.length > 0) {
    throw new Error(
      `Browser test control cleanup failed; refusing to apply a scoped run. ${
        errors.join(" | ")
      }`,
    );
  }
  return consumed;
}

function shouldLoadPagesTests(requestedTestFiles: Set<string>): boolean {
  if (requestedTestFiles.size === 0) {
    return true;
  }
  for (const file of requestedTestFiles) {
    if (file.startsWith("browser-features/pages-")) {
      return true;
    }
  }
  return false;
}

function currentSubtestTimeoutDetail(): string {
  const progress = globalThis.__NORA_TEST_PROGRESS__;
  if (!progress || progress.status !== "running") {
    return "";
  }

  const elapsedMs = Math.max(0, Date.now() - progress.startedAtMs);
  return ` while running ${progress.moduleName} [${progress.index}/${progress.total}] ${progress.testName} (${elapsedMs}ms)`;
}

function withTimeout<T>(
  promise: Promise<T>,
  ms: number,
  timeoutDetail: () => string = () => "",
): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      reject(
        new TimeoutError(`Test timed out after ${ms}ms${timeoutDetail()}`),
      );
    }, ms);

    promise.then(
      (value) => {
        clearTimeout(timeoutId);
        resolve(value);
      },
      (error) => {
        clearTimeout(timeoutId);
        reject(error);
      },
    );
  });
}

export async function runSingleTest(
  file: string,
  loader: LazyModule,
): Promise<TestResult> {
  const started = nowMs();
  let mode: TestResult["mode"] = "import";
  let timedOut = false;
  const mozillaTasks = new MozillaTaskContext(file);
  let marker: DownloadedFirefoxTestMarker | undefined;
  let prefBranch: PrefBranchLike | undefined;
  const prefSnapshots: PrefSnapshot[] = [];
  const errors: string[] = [];
  let cleanupsHandled = false;

  try {
    mozillaTasks.install();
    const loaded = await withTimeout(loader(), PER_TEST_TIMEOUT_MS);
    const mod = normalizeLoadedModule(loaded);
    marker = readDownloadedFirefoxTestMarker(mod);

    if (marker) {
      mode = "mozillaTasks";
      prefBranch = browserPrefs();
      for (const pref of marker.prefs) {
        prefSnapshots.push(snapshotPref(prefBranch, pref.name));
      }
      for (const pref of marker.prefs) {
        setTypedPref(prefBranch, pref);
      }

      await withTimeout(
        marker.load(),
        PER_TEST_TIMEOUT_MS,
        currentSubtestTimeoutDetail,
      );
      if (mozillaTasks.taskCount !== marker.expectedTaskCount) {
        throw new Error(
          `${marker.upstreamPath} registered ${mozillaTasks.taskCount} Mozilla task(s); expected ${marker.expectedTaskCount}`,
        );
      }

      cleanupsHandled = true;
      await withTimeout(
        mozillaTasks.runTasks(),
        PER_TEST_TIMEOUT_MS,
        currentSubtestTimeoutDetail,
      );
    } else {
      const runAllTests = mod.runAllTests;
      if (typeof runAllTests === "function") {
        mode = "runAllTests";
        const result = await withTimeout(
          Promise.resolve((runAllTests as () => unknown)()),
          PER_TEST_TIMEOUT_MS,
          currentSubtestTimeoutDetail,
        );
        if (result === false) {
          throw new Error("runAllTests returned false");
        }
        cleanupsHandled = true;
        await withTimeout(
          mozillaTasks.cleanupAfterImportOnly(),
          PER_TEST_TIMEOUT_MS,
        );
      } else if (mozillaTasks.taskCount > 0) {
        mode = "mozillaTasks";
        cleanupsHandled = true;
        await withTimeout(
          mozillaTasks.runTasks(),
          PER_TEST_TIMEOUT_MS,
          currentSubtestTimeoutDetail,
        );
      } else {
        // Import-only smoke tests are an existing Floorp contract. Only
        // marked downloaded tests require a non-zero exact task count.
        cleanupsHandled = true;
        await withTimeout(
          mozillaTasks.cleanupAfterImportOnly(),
          PER_TEST_TIMEOUT_MS,
        );
      }
    }
  } catch (error) {
    timedOut = error instanceof TimeoutError;
    errors.push(error instanceof Error ? error.message : String(error));
    if (!cleanupsHandled) {
      cleanupsHandled = true;
      try {
        await withTimeout(
          mozillaTasks.cleanupAfterImportOnly(),
          PER_TEST_TIMEOUT_MS,
        );
      } catch (cleanupError) {
        errors.push(
          cleanupError instanceof Error
            ? cleanupError.message
            : String(cleanupError),
        );
      }
    }
  } finally {
    globalThis.__NORA_TEST_PROGRESS__ = undefined;
    try {
      mozillaTasks.restore();
    } catch (restoreError) {
      errors.push(
        `restore Mozilla globals: ${
          restoreError instanceof Error
            ? restoreError.message
            : String(restoreError)
        }`,
      );
    }
    if (prefBranch) {
      errors.push(...restorePrefs(prefBranch, prefSnapshots));
    }
  }

  const tasks = mozillaTasks.taskResults.length > 0
    ? [...mozillaTasks.taskResults]
    : undefined;
  return {
    file,
    ok: errors.length === 0,
    durationMs: Math.round(nowMs() - started),
    mode,
    source: marker ? "downloaded-firefox" : undefined,
    upstreamPath: marker?.upstreamPath,
    manifestPath: marker?.manifestPath,
    tasks,
    timedOut: timedOut || undefined,
    error: errors.length > 0 ? errors.join(" | ") : undefined,
  };
}

export default async function runBrowserTests(): Promise<void> {
  const control = await consumeRequestedTestControl();
  const runId = control.runId;
  const requestedTestFiles = control.requestedTestFiles;
  const results: TestResult[] = [];
  const discoveredFiles: string[] = [];
  const publishRunState = (state: Omit<TestState, "runId">): void => {
    publishState(runId ? { ...state, runId } : state);
  };

  try {
    publishRunState({ status: "running", results, discoveredFiles });

    // Chrome layer tests (via #features-chrome alias)
    const chromeTests = import.meta.glob(
      "#features-chrome/**/test/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    // ESM layer tests (via #features-modules alias)
    const esmTests = import.meta.glob(
      "#features-modules/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    const allTests: Record<string, LazyModule> = {
      ...chromeTests,
      ...esmTests,
    };
    if (shouldLoadPagesTests(requestedTestFiles)) {
      Object.assign(
        allTests,
        (await import("./pages_tests.ts")).getPagesTests(),
      );
    }

    const entries = (Object.entries(allTests) as Array<[string, LazyModule]>)
      .filter(([file]) =>
        requestedTestFiles.size === 0 ||
        requestedTestFiles.has(normalizeBrowserTestPath(file))
      );
    discoveredFiles.push(
      ...entries.map(([file]) => file).sort((a, b) => a.localeCompare(b)),
    );
    publishRunState({ status: "running", results, discoveredFiles });

    console.log(`[nora@test] Found ${entries.length} browser test file(s).`);

    let aborted = false;
    let abortReason: string | undefined;
    for (const [file, loader] of entries) {
      console.log(`[nora@test] Running ${file}`);
      const result = await runSingleTest(file, loader);
      await restorePrimaryBrowserWindow();
      results.push(result);
      publishRunState({ status: "running", results, discoveredFiles });

      if (result.ok) {
        console.log(
          `[nora@test] \u2713 ${file} (${result.mode}, ${result.durationMs}ms)`,
        );
      } else {
        console.error(
          `[nora@test] \u2717 ${file} (${result.durationMs}ms): ${result.error}`,
        );
      }

      if (result.timedOut) {
        aborted = true;
        abortReason =
          `Aborting remaining tests after ${file} timed out; continuing in the same browser could contaminate later results.`;
        console.error(`[nora@test] ${abortReason}`);
        break;
      }
    }

    const passed = results.filter((r) => r.ok).length;
    const failed = results.length - passed;
    const skipped = aborted ? entries.length - results.length : 0;
    console.log(
      `[nora@test] Done: ${passed} passed, ${failed} failed${
        skipped > 0 ? `, ${skipped} skipped` : ""
      }`,
    );

    publishRunState({
      status: "done",
      results,
      discoveredFiles,
      aborted,
      abortReason,
    });

    // Keep the browser alive so the host-side test runner has time to
    // collect results from prefs.js. Without this, the browser may shut
    // down immediately after test completion, before the runner reads the
    // final state. The test runner stops the browser after collecting results.
    // The interval is cleared after 10 minutes as a safety net to prevent
    // the browser from running indefinitely if the host runner fails to
    // shut it down.
    console.log("[nora@test] Keeping browser alive for result collection...");
    const keepaliveId = setInterval(() => {}, 60_000);
    setTimeout(() => clearInterval(keepaliveId), 600_000);
  } catch (e: unknown) {
    const msg = e instanceof Error ? e.message : String(e);
    console.error(`[nora@test] Fatal error: ${msg}`);
    publishRunState({
      status: "error",
      results,
      discoveredFiles,
      error: msg,
    });
  }
}
