/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API around workers

const { TargetList } = require("devtools/shared/resources/target-list");
const { TYPES } = TargetList;

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const IFRAME_URL = URL_ROOT_ORG_SSL + "fission_iframe.html";
const WORKER_FILE = "test_worker.js";
const WORKER_URL = URL_ROOT_SSL + WORKER_FILE;
const IFRAME_WORKER_URL = WORKER_FILE;

add_task(async function() {
  // Enabled fission's pref as the TargetList is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  // The WorkerDebuggerManager#getWorkerDebuggerEnumerator method we're using to retrieve
  // workers loops through _all_ the workers in the process, which means it goes over workers
  // from other tabs as well. Here we add a few tab that are not going to be used in the
  //  test, just to check that their workers won't be retrieved by getAllTargets/watchTargets.
  await addTab(`${FISSION_TEST_URL}?id=first-untargetted-tab`);
  await addTab(`${FISSION_TEST_URL}?id=second-untargetted-tab`);

  info("Test TargetList against workers via a tab target");
  const tab = await addTab(FISSION_TEST_URL);

  // Create a TargetList for the tab
  const descriptor = await mainRoot.getTab({ tab });
  const target = await descriptor.getTarget();

  // Ensure attaching the target as BrowsingContextTargetActor.listWorkers
  // assert that the target actor is attached.
  // It isn't clear if this assertion is meaningful?
  await target.attach();

  const targetList = new TargetList(mainRoot, target);

  // Workaround to allow listening for workers in the content toolbox
  // without the fission preferences
  targetList.listenForWorkers = true;

  await targetList.startListening();

  info("Check that getAllTargets only returns dedicated workers");
  const workers = await targetList.getAllTargets([
    TYPES.WORKER,
    TYPES.SHARED_WORKER,
  ]);

  // XXX: This should be modified in Bug 1607778, where we plan to add support for shared workers.
  is(workers.length, 2, "Retrieved two worker…");
  const mainPageWorker = workers.find(
    worker => worker.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorker = workers.find(
    worker => worker.url == `${IFRAME_WORKER_URL}#simple-worker-in-iframe`
  );
  ok(mainPageWorker, "…the dedicated worker on the main page");
  ok(iframeWorker, "…and the dedicated worker on the iframe");

  info(
    "Assert that watchTargets will call the create callback for existing dedicated workers"
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
  };
  const onDestroy = async ({ targetFront }) => {
    is(
      targetFront.targetType,
      TYPES.WORKER,
      "We are only notified about worker targets"
    );
    ok(!targetFront.isTopLevel, "The workers are never top level");
    destroyedTargets.push(targetFront);
  };

  await targetList.watchTargets(
    [TYPES.WORKER, TYPES.SHARED_WORKER],
    onAvailable,
    onDestroy
  );

  // XXX: This should be modified in Bug 1607778, where we plan to add support for shared workers.
  info("Check that watched targets return the same fronts as getAllTargets");
  is(targets.length, 2, "watcheTargets retrieved 2 worker…");
  const mainPageWorkerTarget = targets.find(t => t === mainPageWorker);
  const iframeWorkerTarget = targets.find(t => t === iframeWorker);

  ok(
    mainPageWorkerTarget,
    "…the dedicated worker in main page, which is the same front we received from getAllTargets"
  );
  ok(
    iframeWorkerTarget,
    "…the dedicated worker in iframe, which is the same front we received from getAllTargets"
  );

  info("Spawn workers in main page and iframe");
  await SpecialPowers.spawn(tab.linkedBrowser, [WORKER_FILE], workerUrl => {
    // Put the worker on the global so we can access it later
    content.spawnedWorker = new content.Worker(`${workerUrl}#spawned-worker`);
    const iframe = content.document.querySelector("iframe");
    SpecialPowers.spawn(iframe, [workerUrl], innerWorkerUrl => {
      // Put the worker on the global so we can access it later
      content.spawnedWorker = new content.Worker(
        `${innerWorkerUrl}#spawned-worker-in-iframe`
      );
    });
  });

  await TestUtils.waitForCondition(
    () => targets.length === 4,
    "Wait for the target list to notify us about the spawned worker"
  );
  const mainPageSpawnedWorkerTarget = targets.find(
    innerTarget => innerTarget.url === `${WORKER_FILE}#spawned-worker`
  );
  const iframeSpawnedWorkerTarget = targets.find(
    innerTarget => innerTarget.url === `${WORKER_FILE}#spawned-worker-in-iframe`
  );

  info(
    "Check that the target list calls onDestroy when a worker is terminated"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.spawnedWorker.terminate();
    content.spawnedWorker = null;

    SpecialPowers.spawn(content.document.querySelector("iframe"), [], () => {
      content.spawnedWorker.terminate();
      content.spawnedWorker = null;
    });
  });
  await TestUtils.waitForCondition(
    () => destroyedTargets.length === 2,
    "Wait for the target list to notify us about the terminated workers"
  );
  const destroyedMainPageSpawnedWorkerTarget = destroyedTargets.find(
    innerTarget => innerTarget === mainPageSpawnedWorkerTarget
  );
  const destroyedIframeSpawnedWorkerTarget = destroyedTargets.find(
    innerTarget => innerTarget === iframeSpawnedWorkerTarget
  );

  ok(
    destroyedMainPageSpawnedWorkerTarget,
    "The target list handled the terminated worker"
  );
  ok(
    destroyedIframeSpawnedWorkerTarget,
    "The target list handled the terminated worker in the iframe"
  );

  info(
    "Check that reloading the page will notify about the terminated worker and the new existing one"
  );
  tab.linkedBrowser.reload();

  await TestUtils.waitForCondition(() => {
    return (
      destroyedTargets.find(t => t === mainPageWorkerTarget) &&
      destroyedTargets.find(t => t === iframeWorkerTarget)
    );
  }, "Wait for the target list to notify us about the terminated workers");
  ok(
    true,
    "The target list notified us about all the expected workers being destroyed when reloading"
  );

  await TestUtils.waitForCondition(
    () => targets.length === 6,
    "Wait for the target list to notify us about the new workers"
  );

  const mainPageWorkerTargetAfterReload = targets.find(
    t => t !== mainPageWorkerTarget && t.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorkerTargetAfterReload = targets.find(
    t =>
      t !== iframeWorkerTarget &&
      t.url == `${IFRAME_WORKER_URL}#simple-worker-in-iframe`
  );

  ok(
    mainPageWorkerTargetAfterReload,
    "The target list handled the worker created once the page navigated"
  );
  ok(
    iframeWorkerTargetAfterReload,
    "The target list handled the worker created in the iframe once the page navigated"
  );

  info("Check that target list handles adding an iframe with workers");
  const hashSuffix = "in-created-iframe";
  const iframeUrl = `${IFRAME_URL}?hashSuffix=${hashSuffix}`;
  await SpecialPowers.spawn(tab.linkedBrowser, [iframeUrl], url => {
    const iframe = content.document.createElement("iframe");
    content.document.body.append(iframe);
    iframe.src = url;
  });

  // It's important to check the length of `targets` here to ensure we don't get unwanted
  // worker targets.
  await TestUtils.waitForCondition(
    () => targets.length === 7,
    "Wait for the target list to notify us about the worker in the new iframe"
  );
  const spawnedIframeWorkerTarget = targets.find(
    worker => worker.url == `${WORKER_FILE}#simple-worker-${hashSuffix}`
  );
  ok(
    spawnedIframeWorkerTarget,
    "The target list handled the worker in the new iframe"
  );

  info("Check that navigating away does destroy all targets");
  await BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "data:text/html,<meta charset=utf8>Away"
  );

  await TestUtils.waitForCondition(
    () => destroyedTargets.length === targets.length,
    "Wait for all the targets to be reporeted as destroyed"
  );

  ok(
    destroyedTargets.find(t => t === mainPageWorkerTargetAfterReload),
    "main page worker target was destroyed"
  );
  ok(
    destroyedTargets.find(t => t === iframeWorkerTargetAfterReload),
    "iframe worker target was destroyed"
  );
  ok(
    destroyedTargets.find(t => t === spawnedIframeWorkerTarget),
    "spawned iframe worker target was destroyed"
  );

  targetList.unwatchTargets(
    [TYPES.WORKER, TYPES.SHARED_WORKER],
    onAvailable,
    onDestroy
  );
  targetList.destroy();

  info("Unregister service workers so they don't appear in other tests.");
  await unregisterAllServiceWorkers(client);

  BrowserTestUtils.removeTab(tab);
  await client.close();
});
