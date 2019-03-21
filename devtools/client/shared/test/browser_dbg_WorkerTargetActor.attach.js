/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this);

var MAX_TOTAL_VIEWERS = "browser.sessionhistory.max_total_viewers";

var TAB1_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attach-tab1.html";
var TAB2_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attach-tab2.html";
var WORKER1_URL = "code_WorkerTargetActor.attach-worker1.js";
var WORKER2_URL = "code_WorkerTargetActor.attach-worker2.js";

add_task(async function() {
  const oldMaxTotalViewers = SpecialPowers.getIntPref(MAX_TOTAL_VIEWERS);
  SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, 10);

  const tab = await addTab(TAB1_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  await listWorkers(target);

  // If a page still has pending network requests, it will not be moved into
  // the bfcache. Consequently, we cannot use waitForWorkerListChanged here,
  // because the worker is not guaranteed to have finished loading when it is
  // registered. Instead, we have to wait for the promise returned by
  // createWorker in the tab to be resolved.
  await createWorkerInTab(tab, WORKER1_URL);
  let { workers } = await listWorkers(target);
  let workerTargetFront1 = findWorker(workers, WORKER1_URL);
  await workerTargetFront1.attach();
  ok(workerTargetFront1.actorID, "front for worker in tab 1 has been attached");

  executeSoon(() => {
    BrowserTestUtils.loadURI(tab.linkedBrowser, TAB2_URL);
  });
  await waitForWorkerClose(workerTargetFront1);
  ok(!!workerTargetFront1.actorID, "front for worker in tab 1 has been closed");

  await createWorkerInTab(tab, WORKER2_URL);
  ({ workers } = await listWorkers(target));
  const workerTargetFront2 = findWorker(workers, WORKER2_URL);
  await workerTargetFront2.attach();
  ok(workerTargetFront2.actorID, "front for worker in tab 2 has been attached");

  executeSoon(() => {
    tab.linkedBrowser.goBack();
  });
  await waitForWorkerClose(workerTargetFront2);
  ok(!!workerTargetFront2.actorID, "front for worker in tab 2 has been closed");

  ({ workers } = await listWorkers(target));
  workerTargetFront1 = findWorker(workers, WORKER1_URL);
  await workerTargetFront1.attach();
  ok(workerTargetFront1.actorID, "front for worker in tab 1 has been attached");

  await target.destroy();
  SpecialPowers.setIntPref(MAX_TOTAL_VIEWERS, oldMaxTotalViewers);
  finish();
});
