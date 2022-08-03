/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test WORKER targets when doing history navigations (BF Cache)
//
// Use a distinct file as this test currently hits a DEBUG assertion
// https://searchfox.org/mozilla-central/rev/352b525ab841278cd9b3098343f655ef85933544/dom/workers/WorkerPrivate.cpp#5218
// and so is running only on OPT builds.

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const WORKER_FILE = "test_worker.js";
const WORKER_URL = URL_ROOT_SSL + WORKER_FILE;
const IFRAME_WORKER_URL = URL_ROOT_ORG_SSL + WORKER_FILE;

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  // The WorkerDebuggerManager#getWorkerDebuggerEnumerator method we're using to retrieve
  // workers loops through _all_ the workers in the process, which means it goes over workers
  // from other tabs as well. Here we add a few tabs that are not going to be used in the
  // test, just to check that their workers won't be retrieved by getAllTargets/watchTargets.
  await addTab(`${FISSION_TEST_URL}?id=first-untargetted-tab&noServiceWorker`);
  await addTab(`${FISSION_TEST_URL}?id=second-untargetted-tab&noServiceWorker`);

  info("Test bfcache navigations");
  const tab = await addTab(`${FISSION_TEST_URL}?&noServiceWorker`);

  // Create a TargetCommand for the tab
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  // Workaround to allow listening for workers in the content toolbox
  // without the fission preferences
  targetCommand.listenForWorkers = true;

  await targetCommand.startListening();

  const { TYPES } = targetCommand;

  info(
    "Assert that watchTargets will call the onAvailable callback for existing dedicated workers"
  );
  const targets = [];
  const destroyedTargets = [];
  const onAvailable = async ({ targetFront }) => {
    info(`onAvailable called for ${targetFront.url}`);
    is(
      targetFront.targetType,
      TYPES.WORKER,
      "We are only notified about worker targets"
    );
    ok(!targetFront.isTopLevel, "The workers are never top level");
    targets.push(targetFront);
    info(`Handled ${targets.length} new targets`);
  };
  const onDestroyed = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.WORKER,
      "We are only notified about worker targets"
    );
    ok(!targetFront.isTopLevel, "The workers are never top level");
    destroyedTargets.push(targetFront);
  };

  await targetCommand.watchTargets({
    types: [TYPES.WORKER, TYPES.SHARED_WORKER],
    onAvailable,
    onDestroyed,
  });

  is(targets.length, 2, "watchTargets retrieved 2 workers…");
  const mainPageWorkerTarget = targets.find(
    worker => worker.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorkerTarget = targets.find(
    worker => worker.url == `${IFRAME_WORKER_URL}#simple-worker-in-iframe`
  );

  ok(
    mainPageWorkerTarget,
    "…the dedicated worker in main page, which is the same front we received from getAllTargets"
  );
  ok(
    iframeWorkerTarget,
    "…the dedicated worker in iframe, which is the same front we received from getAllTargets"
  );

  info("Check that navigating away does destroy all targets");
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "data:text/html,<meta charset=utf8>Away"
  );
  await onBrowserLoaded;

  await waitFor(
    () => destroyedTargets.length === 2,
    "Wait for all the targets to be reported as destroyed"
  );

  info("Navigate back to the first page");
  gBrowser.goBack();

  await waitFor(
    () => targets.length === 4,
    "Wait for the target list to notify us about the first page workers, restored from the BF Cache"
  );

  const mainPageWorkerTargetAfterGoingBack = targets.find(
    t => t !== mainPageWorkerTarget && t.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorkerTargetAfterGoingBack = targets.find(
    t =>
      t !== iframeWorkerTarget &&
      t.url == `${IFRAME_WORKER_URL}#simple-worker-in-iframe`
  );

  ok(
    mainPageWorkerTargetAfterGoingBack,
    "The target list handled the worker created from the BF Cache"
  );
  ok(
    iframeWorkerTargetAfterGoingBack,
    "The target list handled the worker created in the iframe from the BF Cache"
  );
});
