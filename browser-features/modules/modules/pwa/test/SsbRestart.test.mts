// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import type { Manifest } from "#features-chrome/common/pwa/type.ts";
import type {
  RestartEntry,
  SsbRestartDependencies,
} from "../SsbRestart.sys.mts";

const { SsbRestartManager, restartUrl, parseRestartEntries } = ChromeUtils
  .importESModule(
    "resource://noraneko/modules/pwa/SsbRestart.sys.mjs",
  ) as typeof import("../SsbRestart.sys.mts");

function fixture() {
  let pending = "";
  let restarting = false;
  let wasRestarted = false;
  let enabled = true;
  let windows: RestartEntry[] = [];
  const launches: Array<{ id: string; url: string; pendingAtOpen: string }> =
    [];
  let flushes = 0;
  const apps: Record<string, Manifest> = {
    alpha: {
      id: "alpha",
      name: "Alpha",
      start_url: "https://example.test/app/",
      icon: "",
    },
    beta: {
      id: "beta",
      name: "Beta",
      start_url: "https://other.test/",
      icon: "",
    },
  };
  const deps: SsbRestartDependencies = {
    restarting: () => restarting,
    wasRestarted: () => wasRestarted,
    enabled: () => enabled,
    windows: () => windows.slice(),
    read: () => pending,
    write: (value) => {
      pending = value;
    },
    clear: () => {
      pending = "";
    },
    flush: () => {
      flushes++;
    },
    getApp: (id) => Promise.resolve(apps[id] ?? null),
    openApp: (app, url) => {
      launches.push({ id: app.id, url, pendingAtOpen: pending });
      windows.push({ id: app.id, url });
      return Promise.resolve();
    },
  };
  return {
    deps,
    launches,
    setPending: (value: string) => {
      pending = value;
    },
    getPending: () => pending,
    setRestarting: (value: boolean) => {
      restarting = value;
    },
    setWasRestarted: (value: boolean) => {
      wasRestarted = value;
    },
    setEnabled: (value: boolean) => {
      enabled = value;
    },
    setWindows: (value: RestartEntry[]) => {
      windows = value;
    },
    getFlushes: () => flushes,
  };
}

function testNormalQuitDoesNotQueueWindows(): void {
  const f = fixture();
  f.setPending('[{"id":"stale","url":""}]');
  f.setWindows([{ id: "alpha", url: "https://example.test/app/" }]);
  new SsbRestartManager(f.deps).saveForRestart();
  assertEquals(
    f.getPending(),
    "",
    "normal quit clears any stale restart request",
  );
  assertEquals(f.getFlushes(), 1, "shutdown state is flushed");
}

async function testRestartRestoresOnlyOpenAppsOnce(): Promise<void> {
  const f = fixture();
  const entries = [
    { id: "alpha", url: "https://example.test/app/one?q=1" },
    { id: "alpha", url: "https://example.test/app/two" },
    { id: "beta", url: "https://other.test/previous" },
    { id: "uninstalled", url: "https://gone.test/" },
  ];
  f.setRestarting(true);
  f.setWindows(entries);
  const manager = new SsbRestartManager(f.deps);
  manager.saveForRestart();
  assertEquals(
    f.getPending(),
    JSON.stringify(entries),
    "snapshot keeps individual windows",
  );
  f.setWasRestarted(true);
  f.setWindows([]);
  await manager.restoreAfterRestart();
  assertEquals(
    f.launches.map(({ id }) => id).join(","),
    "alpha,alpha,beta",
    "only registered open apps return, including duplicate windows",
  );
  assertEquals(
    f.launches[0].url,
    entries[0].url,
    "same-origin page survives restart",
  );
  assert(
    f.launches.every((launch) => launch.pendingAtOpen === ""),
    "request is consumed before any window opens",
  );
  await manager.restoreAfterRestart();
  assertEquals(
    f.launches.length,
    3,
    "a second initialization cannot replay the request",
  );
}

async function testExistingWindowDoesNotConsumeRestore(): Promise<void> {
  const f = fixture();
  f.setWasRestarted(true);
  f.setPending(
    JSON.stringify([{ id: "alpha", url: "https://example.test/app/1" }, {
      id: "alpha",
      url: "https://example.test/app/2",
    }]),
  );
  f.setWindows([{ id: "alpha", url: "https://example.test/app/already-open" }]);
  await new SsbRestartManager(f.deps).restoreAfterRestart();
  assertEquals(
    f.launches.length,
    2,
    "an independently opened window does not consume a queued restore",
  );
}

async function testNormalStartAndDisabledFeatureDoNotReplay(): Promise<void> {
  const f = fixture();
  const value = JSON.stringify([{
    id: "alpha",
    url: "https://example.test/app/",
  }]);
  f.setPending(value);
  await new SsbRestartManager(f.deps).restoreAfterRestart();
  assertEquals(f.getPending(), "", "normal start consumes stale state");
  f.setPending(value);
  f.setWasRestarted(true);
  f.setEnabled(false);
  await new SsbRestartManager(f.deps).restoreAfterRestart();
  assertEquals(f.getPending(), "", "disabled PWA consumes stale state");
  assertEquals(f.launches.length, 0, "neither case launches an app");
  f.setRestarting(true);
  f.setWindows([{ id: "alpha", url: "https://example.test/app/" }]);
  new SsbRestartManager(f.deps).saveForRestart();
  assertEquals(f.getPending(), "", "disabled PWA does not queue at shutdown");
}

async function testCannotFlushDoesNotOpenApps(): Promise<void> {
  const f = fixture();
  f.setWasRestarted(true);
  f.setPending(JSON.stringify([{ id: "alpha", url: "https://example.test/" }]));
  f.deps.flush = () => {
    throw new Error("read-only profile");
  };
  await new SsbRestartManager(f.deps).restoreAfterRestart();
  assertEquals(
    f.launches.length,
    0,
    "do not replay a request that could not be cleared on disk",
  );
}

function testInvalidDataAndCrossOrigin(): void {
  assertEquals(
    parseRestartEntries("{not json").length,
    0,
    "corrupt state is ignored",
  );
  assertEquals(
    parseRestartEntries('[{"id":"","url":"x"},{"id":"a","url":4}]').length,
    0,
    "malformed entries are ignored",
  );
  assertEquals(
    restartUrl("https://example.test/app/", "https://elsewhere.test/"),
    "https://example.test/app/",
    "external pages fall back to registered start URL",
  );
  assertEquals(
    restartUrl("https://example.test/app/", "file:///tmp/private"),
    "https://example.test/app/",
    "non-web URLs never replay",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("SsbRestart.test.mts", [
    {
      name: "normal quit does not queue windows",
      fn: testNormalQuitDoesNotQueueWindows,
    },
    {
      name: "restart restores open registered apps once",
      fn: testRestartRestoresOnlyOpenAppsOnce,
    },
    {
      name: "existing app instance does not consume a queued restore",
      fn: testExistingWindowDoesNotConsumeRestore,
    },
    {
      name: "normal start and disabled feature do not replay",
      fn: testNormalStartAndDisabledFeatureDoNotReplay,
    },
    {
      name: "a failed queue flush never opens an app",
      fn: testCannotFlushDoesNotOpenApps,
    },
    {
      name: "invalid state and external URLs are rejected",
      fn: testInvalidDataAndCrossOrigin,
    },
  ]);
}
await runAllTests();
