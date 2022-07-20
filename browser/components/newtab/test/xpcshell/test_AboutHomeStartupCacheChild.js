/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AboutHomeStartupCacheChild } = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTabService.sys.mjs"
);

/**
 * Tests that AboutHomeStartupCacheChild will terminate its PromiseWorker
 * on memory-pressure, and that a new PromiseWorker can then be generated on
 * demand.
 */
add_task(async function test_memory_pressure() {
  AboutHomeStartupCacheChild.init();

  let worker = AboutHomeStartupCacheChild.getOrCreateWorker();
  Assert.ok(worker, "Should have been able to get the worker.");

  Assert.equal(
    worker,
    AboutHomeStartupCacheChild.getOrCreateWorker(),
    "The worker is cached and re-usable."
  );

  Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

  let newWorker = AboutHomeStartupCacheChild.getOrCreateWorker();
  Assert.notEqual(worker, newWorker, "Old worker should have been replaced.");

  AboutHomeStartupCacheChild.uninit();
});
