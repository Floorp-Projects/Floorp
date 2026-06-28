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

import { MozillaTaskContext } from "./mochitest_compat.ts";

interface TestResult {
  file: string;
  ok: boolean;
  durationMs: number;
  mode: "import" | "runAllTests" | "mozillaTasks";
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

declare global {
  var __TEST_RESULTS__: TestState | undefined;
  var __NORA_TEST_PROGRESS__:
    | {
      moduleName: string;
      testName: string;
      status: string;
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

type LazyModule = () => Promise<unknown>;

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

async function readRequestedTestFilesFromControlFile(
  runId: string | undefined,
): Promise<Set<string> | undefined> {
  try {
    const globals = globalThis as Record<string, unknown>;
    const services = globals.Services;
    const dirsvc = isRecord(services) ? services.dirsvc : undefined;
    const get = callableMethod(dirsvc, "get");
    const profileDir = get?.call(dirsvc, "ProfD", globals.Ci);
    const profilePath = isRecord(profileDir) ? profileDir.path : undefined;
    if (typeof profilePath !== "string") {
      return undefined;
    }

    const pathUtils = globals.PathUtils;
    const join = callableMethod(pathUtils, "join");
    const ioUtils = globals.IOUtils;
    const readUTF8 = callableMethod(ioUtils, "readUTF8");
    if (!isRecord(pathUtils) || !join || !isRecord(ioUtils) || !readUTF8) {
      return undefined;
    }

    const controlPath = join.call(pathUtils, profilePath, TEST_CONTROL_FILE);
    if (typeof controlPath !== "string") {
      return undefined;
    }

    const raw = await readUTF8.call(ioUtils, controlPath);
    if (typeof raw !== "string") {
      return undefined;
    }

    const parsed: unknown = JSON.parse(raw);
    if (!isRecord(parsed)) {
      return undefined;
    }
    if (
      runId && typeof parsed.runId === "string" && parsed.runId !== runId
    ) {
      return undefined;
    }
    return setFromFilterEntries(parsed.filter);
  } catch {
    return undefined;
  }
}

async function readRequestedTestFiles(
  runId: string | undefined,
): Promise<Set<string>> {
  try {
    const services = (globalThis as Record<string, unknown>).Services as
      | {
        prefs?: {
          getStringPref: (name: string, fallback?: string) => string;
        };
      }
      | undefined;
    const prefs = services?.prefs;
    const countRaw = prefs?.getStringPref(TEST_FILTER_COUNT_PREF, "");
    const count = Number(countRaw);
    if (Number.isInteger(count) && count >= 0) {
      const entries: string[] = [];
      for (let index = 0; index < count; index++) {
        const entry = prefs?.getStringPref(
          `${TEST_FILTER_ITEM_PREF_PREFIX}${index}`,
          "",
        );
        if (entry) {
          entries.push(entry);
        }
      }
      return setFromFilterEntries(entries) ?? new Set();
    }
  } catch {
    // Fall back to the legacy single-pref or control-file paths below.
  }

  const controlFileFilter = await readRequestedTestFilesFromControlFile(runId);
  if (controlFileFilter) {
    return controlFileFilter;
  }

  try {
    const services = (globalThis as Record<string, unknown>).Services as
      | {
        prefs?: {
          getStringPref: (name: string, fallback?: string) => string;
          setStringPref: (name: string, value: string) => void;
          savePrefFile?: (prefFile: string | null) => void;
        };
      }
      | undefined;
    const prefs = services?.prefs;
    const raw = prefs?.getStringPref(TEST_FILTER_PREF, "[]") ?? "[]";
    const parsed: unknown = JSON.parse(raw);
    try {
      prefs?.setStringPref(TEST_FILTER_PREF, "[]");
      prefs?.savePrefFile?.(null);
    } catch {
      // Clearing the one-shot filter is best-effort; parsing already succeeded.
    }
    if (!Array.isArray(parsed)) {
      return new Set();
    }
    return setFromFilterEntries(parsed) ?? new Set();
  } catch {
    return new Set();
  }
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

function readTestRunId(): string | undefined {
  try {
    const services = (globalThis as Record<string, unknown>).Services as
      | {
        prefs?: {
          getStringPref: (name: string, fallback?: string) => string;
        };
      }
      | undefined;
    const value = services?.prefs?.getStringPref(TEST_RUN_ID_PREF, "") ?? "";
    return value || undefined;
  } catch {
    return undefined;
  }
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

async function runSingleTest(
  file: string,
  loader: LazyModule,
): Promise<TestResult> {
  const started = nowMs();
  let mode: TestResult["mode"] = "import";
  let timedOut = false;
  const mozillaTasks = new MozillaTaskContext(file);

  try {
    mozillaTasks.install();
    const loaded = await withTimeout(loader(), PER_TEST_TIMEOUT_MS);
    const mod = normalizeLoadedModule(loaded);
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
      await withTimeout(
        mozillaTasks.cleanupAfterImportOnly(),
        PER_TEST_TIMEOUT_MS,
      );
      return {
        file,
        ok: true,
        durationMs: Math.round(nowMs() - started),
        mode,
      };
    }

    if (mozillaTasks.taskCount > 0) {
      mode = "mozillaTasks";
      await withTimeout(mozillaTasks.runTasks(), PER_TEST_TIMEOUT_MS);
      return {
        file,
        ok: true,
        durationMs: Math.round(nowMs() - started),
        mode,
      };
    }

    await withTimeout(
      mozillaTasks.cleanupAfterImportOnly(),
      PER_TEST_TIMEOUT_MS,
    );
    return {
      file,
      ok: true,
      durationMs: Math.round(nowMs() - started),
      mode,
    };
  } catch (e: unknown) {
    const errorMessage = e instanceof Error ? e.message : String(e);
    timedOut = e instanceof TimeoutError;
    let cleanupMessage = "";
    if (mode !== "mozillaTasks") {
      try {
        await mozillaTasks.cleanupAfterImportOnly();
      } catch (cleanupError) {
        cleanupMessage = cleanupError instanceof Error
          ? cleanupError.message
          : String(cleanupError);
      }
    }

    return {
      file,
      ok: false,
      durationMs: Math.round(nowMs() - started),
      mode,
      timedOut,
      error: cleanupMessage
        ? `${errorMessage} | ${cleanupMessage}`
        : errorMessage,
    };
  } finally {
    mozillaTasks.restore();
  }
}

export default async function runBrowserTests(): Promise<void> {
  const results: TestResult[] = [];
  const discoveredFiles: string[] = [];
  const runId = readTestRunId();
  const publishRunState = (state: Omit<TestState, "runId">): void => {
    publishState(runId ? { ...state, runId } : state);
  };
  publishRunState({ status: "running", results, discoveredFiles });

  try {
    // Chrome layer tests (via #features-chrome alias)
    const chromeTests = import.meta.glob(
      "#features-chrome/**/test/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    // ESM layer tests (via #features-modules alias)
    const esmTests = import.meta.glob(
      "#features-modules/**/*.test.{ts,mts,tsx,js,mjs,jsx}",
    );

    const requestedTestFiles = await readRequestedTestFiles(runId);
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
