/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around workers

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const IFRAME_FILE = "fission_iframe.html";
const REMOTE_IFRAME_URL = URL_ROOT_ORG_SSL + IFRAME_FILE;
const IFRAME_URL = URL_ROOT_SSL + IFRAME_FILE;
const WORKER_FILE = "test_worker.js";
const WORKER_URL = URL_ROOT_SSL + WORKER_FILE;
const REMOTE_IFRAME_WORKER_URL = URL_ROOT_ORG_SSL + WORKER_FILE;

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

  info("Test TargetCommand against workers via a tab target");
  const tab = await addTab(`${FISSION_TEST_URL}?&noServiceWorker`);

  // Create a TargetCommand for the tab
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  // Workaround to allow listening for workers in the content toolbox
  // without the fission preferences
  targetCommand.listenForWorkers = true;

  await commands.targetCommand.startListening();

  const { TYPES } = targetCommand;

  info("Check that getAllTargets only returns dedicated workers");
  const workers = await targetCommand.getAllTargets([
    TYPES.WORKER,
    TYPES.SHARED_WORKER,
  ]);

  // XXX: This should be modified in Bug 1607778, where we plan to add support for shared workers.
  is(workers.length, 2, "Retrieved two worker…");
  const mainPageWorker = workers.find(
    worker => worker.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorker = workers.find(worker => {
    return worker.url == `${REMOTE_IFRAME_WORKER_URL}#simple-worker-in-iframe`;
  });
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
    info(`Handled ${targets.length} targets\n`);
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

  await waitFor(
    () => targets.length === 4,
    "Wait for the target list to notify us about the spawned worker"
  );
  const mainPageSpawnedWorkerTarget = targets.find(
    innerTarget => innerTarget.url == `${WORKER_URL}#spawned-worker`
  );
  ok(mainPageSpawnedWorkerTarget, "Retrieved spawned worker");
  const iframeSpawnedWorkerTarget = targets.find(
    innerTarget =>
      innerTarget.url == `${REMOTE_IFRAME_WORKER_URL}#spawned-worker-in-iframe`
  );
  ok(iframeSpawnedWorkerTarget, "Retrieved spawned worker in iframe");

  await wait(100);

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
  await waitFor(
    () =>
      destroyedTargets.includes(mainPageSpawnedWorkerTarget) &&
      destroyedTargets.includes(iframeSpawnedWorkerTarget),
    "Wait for the target list to notify us about the terminated workers"
  );

  ok(
    true,
    "The target list handled the terminated workers (from the main page and the iframe)"
  );

  info(
    "Check that reloading the page will notify about the terminated worker and the new existing one"
  );
  const targetsCountBeforeReload = targets.length;
  await reloadBrowser();

  await waitFor(() => {
    return (
      destroyedTargets.includes(mainPageWorkerTarget) &&
      destroyedTargets.includes(iframeWorkerTarget)
    );
  }, `Wait for the target list to notify us about the terminated workers when reloading`);
  ok(
    true,
    "The target list notified us about all the expected workers being destroyed when reloading"
  );

  await waitFor(
    () => targets.length === targetsCountBeforeReload + 2,
    "Wait for the target list to notify us about the new workers after reloading"
  );

  const mainPageWorkerTargetAfterReload = targets.find(
    t => t !== mainPageWorkerTarget && t.url == `${WORKER_URL}#simple-worker`
  );
  const iframeWorkerTargetAfterReload = targets.find(
    t =>
      t !== iframeWorkerTarget &&
      t.url == `${REMOTE_IFRAME_WORKER_URL}#simple-worker-in-iframe`
  );

  ok(
    mainPageWorkerTargetAfterReload,
    "The target list handled the worker created once the page navigated"
  );
  ok(
    iframeWorkerTargetAfterReload,
    "The target list handled the worker created in the iframe once the page navigated"
  );

  const targetCount = targets.length;

  info(
    "Check that when removing an iframe we're notified about its workers being terminated"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.querySelector("iframe").remove();
  });
  await waitFor(() => {
    return destroyedTargets.includes(iframeWorkerTargetAfterReload);
  }, `Wait for the target list to notify us about the terminated workers when removing an iframe`);

  info("Check that target list handles adding iframes with workers");
  const iframeUrl = `${IFRAME_URL}?noServiceWorker=true&hashSuffix=in-created-iframe`;
  const remoteIframeUrl = `${REMOTE_IFRAME_URL}?noServiceWorker=true&hashSuffix=in-created-remote-iframe`;

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [iframeUrl, remoteIframeUrl],
    (url, remoteUrl) => {
      const firstIframe = content.document.createElement("iframe");
      content.document.body.append(firstIframe);
      firstIframe.src = url + "-1";

      const secondIframe = content.document.createElement("iframe");
      content.document.body.append(secondIframe);
      secondIframe.src = url + "-2";

      const firstRemoteIframe = content.document.createElement("iframe");
      content.document.body.append(firstRemoteIframe);
      firstRemoteIframe.src = remoteUrl + "-1";

      const secondRemoteIframe = content.document.createElement("iframe");
      content.document.body.append(secondRemoteIframe);
      secondRemoteIframe.src = remoteUrl + "-2";
    }
  );

  // It's important to check the length of `targets` here to ensure we don't get unwanted
  // worker targets.
  await waitFor(
    () => targets.length === targetCount + 4,
    "Wait for the target list to notify us about the workers in the new iframes"
  );
  const firstSpawnedIframeWorkerTarget = targets.find(
    worker => worker.url == `${WORKER_URL}#simple-worker-in-created-iframe-1`
  );
  const secondSpawnedIframeWorkerTarget = targets.find(
    worker => worker.url == `${WORKER_URL}#simple-worker-in-created-iframe-2`
  );
  const firstSpawnedRemoteIframeWorkerTarget = targets.find(
    worker =>
      worker.url ==
      `${REMOTE_IFRAME_WORKER_URL}#simple-worker-in-created-remote-iframe-1`
  );
  const secondSpawnedRemoteIframeWorkerTarget = targets.find(
    worker =>
      worker.url ==
      `${REMOTE_IFRAME_WORKER_URL}#simple-worker-in-created-remote-iframe-2`
  );

  ok(
    firstSpawnedIframeWorkerTarget,
    "The target list handled the worker in the first new same-origin iframe"
  );
  ok(
    secondSpawnedIframeWorkerTarget,
    "The target list handled the worker in the second new same-origin iframe"
  );
  ok(
    firstSpawnedRemoteIframeWorkerTarget,
    "The target list handled the worker in the first new remote iframe"
  );
  ok(
    secondSpawnedRemoteIframeWorkerTarget,
    "The target list handled the worker in the second new remote iframe"
  );

  info("Check that navigating away does destroy all targets");
  BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "data:text/html,<meta charset=utf8>Away"
  );

  await waitFor(
    () => destroyedTargets.length === targets.length,
    "Wait for all the targets to be reported as destroyed"
  );

  ok(
    destroyedTargets.includes(mainPageWorkerTargetAfterReload),
    "main page worker target was destroyed"
  );
  ok(
    destroyedTargets.includes(firstSpawnedIframeWorkerTarget),
    "first spawned same-origin iframe worker target was destroyed"
  );
  ok(
    destroyedTargets.includes(secondSpawnedIframeWorkerTarget),
    "second spawned same-origin iframe worker target was destroyed"
  );
  ok(
    destroyedTargets.includes(firstSpawnedRemoteIframeWorkerTarget),
    "first spawned remote iframe worker target was destroyed"
  );
  ok(
    destroyedTargets.includes(secondSpawnedRemoteIframeWorkerTarget),
    "second spawned remote iframe worker target was destroyed"
  );

  targetCommand.unwatchTargets({
    types: [TYPES.WORKER, TYPES.SHARED_WORKER],
    onAvailable,
    onDestroyed,
  });
  targetCommand.destroy();

  info("Unregister service workers so they don't appear in other tests.");
  await unregisterAllServiceWorkers(commands.client);

  BrowserTestUtils.removeTab(tab);
  await commands.destroy();
});
