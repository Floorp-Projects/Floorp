/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API around workers

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const WORKER_FILE = "test_worker.js";
const CHROME_WORKER_URL = CHROME_URL_ROOT + WORKER_FILE;
const SERVICE_WORKER_URL = URL_ROOT_SSL + "test_service_worker.js";

add_task(async function() {
  // Enabled fission's pref as the TargetCommand is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const tab = await addTab(FISSION_TEST_URL);

  info("Test TargetCommand against workers via the parent process target");

  // Instantiate a worker in the parent process
  // eslint-disable-next-line no-unused-vars
  const worker = new Worker(CHROME_WORKER_URL + "#simple-worker");
  // eslint-disable-next-line no-unused-vars
  const sharedWorker = new SharedWorker(CHROME_WORKER_URL + "#shared-worker");

  const commands = await CommandsFactory.forMainProcess();
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;
  await targetCommand.startListening();

  // Very naive sanity check against getAllTargets([workerType])
  info("Check that getAllTargets returned the expected targets");
  const workers = await targetCommand.getAllTargets([TYPES.WORKER]);
  const hasWorker = workers.find(workerTarget => {
    return workerTarget.url == CHROME_WORKER_URL + "#simple-worker";
  });
  ok(hasWorker, "retrieve the target for the worker");

  const sharedWorkers = await targetCommand.getAllTargets([
    TYPES.SHARED_WORKER,
  ]);
  const hasSharedWorker = sharedWorkers.find(workerTarget => {
    return workerTarget.url == CHROME_WORKER_URL + "#shared-worker";
  });
  ok(hasSharedWorker, "retrieve the target for the shared worker");

  const serviceWorkers = await targetCommand.getAllTargets([
    TYPES.SERVICE_WORKER,
  ]);
  const hasServiceWorker = serviceWorkers.find(workerTarget => {
    return workerTarget.url == SERVICE_WORKER_URL;
  });
  ok(hasServiceWorker, "retrieve the target for the service worker");

  info(
    "Check that calling getAllTargets again return the same target instances"
  );
  const workers2 = await targetCommand.getAllTargets([TYPES.WORKER]);
  const sharedWorkers2 = await targetCommand.getAllTargets([
    TYPES.SHARED_WORKER,
  ]);
  const serviceWorkers2 = await targetCommand.getAllTargets([
    TYPES.SERVICE_WORKER,
  ]);
  is(workers2.length, workers.length, "retrieved the same number of workers");
  is(
    sharedWorkers2.length,
    sharedWorkers.length,
    "retrieved the same number of shared workers"
  );
  is(
    serviceWorkers2.length,
    serviceWorkers.length,
    "retrieved the same number of service workers"
  );

  workers.sort(sortFronts);
  workers2.sort(sortFronts);
  sharedWorkers.sort(sortFronts);
  sharedWorkers2.sort(sortFronts);
  serviceWorkers.sort(sortFronts);
  serviceWorkers2.sort(sortFronts);

  for (let i = 0; i < workers.length; i++) {
    is(workers[i], workers2[i], `worker ${i} targets are the same`);
  }
  for (let i = 0; i < sharedWorkers2.length; i++) {
    is(
      sharedWorkers[i],
      sharedWorkers2[i],
      `shared worker ${i} targets are the same`
    );
  }
  for (let i = 0; i < serviceWorkers2.length; i++) {
    is(
      serviceWorkers[i],
      serviceWorkers2[i],
      `service worker ${i} targets are the same`
    );
  }

  info(
    "Check that watchTargets will call the create callback for all existing workers"
  );
  const targets = [];
  const topLevelTarget = await commands.targetCommand.targetFront;
  const onAvailable = async ({ targetFront }) => {
    ok(
      targetFront.targetType === TYPES.WORKER ||
        targetFront.targetType === TYPES.SHARED_WORKER ||
        targetFront.targetType === TYPES.SERVICE_WORKER,
      "We are only notified about worker targets"
    );
    ok(
      targetFront == topLevelTarget
        ? targetFront.isTopLevel
        : !targetFront.isTopLevel,
      "isTopLevel property is correct"
    );
    targets.push(targetFront);
  };
  await targetCommand.watchTargets(
    [TYPES.WORKER, TYPES.SHARED_WORKER, TYPES.SERVICE_WORKER],
    onAvailable
  );
  is(
    targets.length,
    workers.length + sharedWorkers.length + serviceWorkers.length,
    "retrieved the same number of workers via watchTargets"
  );

  targets.sort(sortFronts);
  const allWorkers = workers
    .concat(sharedWorkers, serviceWorkers)
    .sort(sortFronts);

  for (let i = 0; i < allWorkers.length; i++) {
    is(
      allWorkers[i],
      targets[i],
      `worker ${i} targets are the same via watchTargets`
    );
  }

  targetCommand.unwatchTargets(
    [TYPES.WORKER, TYPES.SHARED_WORKER, TYPES.SERVICE_WORKER],
    onAvailable
  );

  // Create a new worker and see if the worker target is reported
  const onWorkerCreated = new Promise(resolve => {
    const onAvailable2 = async ({ targetFront }) => {
      if (targets.includes(targetFront)) {
        return;
      }
      targetCommand.unwatchTargets([TYPES.WORKER], onAvailable2);
      resolve(targetFront);
    };
    targetCommand.watchTargets([TYPES.WORKER], onAvailable2);
  });
  // eslint-disable-next-line no-unused-vars
  const worker2 = new Worker(CHROME_WORKER_URL + "#second");
  info("Wait for the second worker to be created");
  const workerTarget = await onWorkerCreated;

  is(
    workerTarget.url,
    CHROME_WORKER_URL + "#second",
    "This worker target is about the new worker"
  );
  is(
    workerTarget.name,
    "test_worker.js#second",
    "The worker target has the expected name"
  );

  const workers3 = await targetCommand.getAllTargets([TYPES.WORKER]);
  const hasWorker2 = workers3.find(
    ({ url }) => url == `${CHROME_WORKER_URL}#second`
  );
  ok(hasWorker2, "retrieve the target for tab via getAllTargets");

  info(
    "Check that terminating the worker does trigger the onDestroyed callback"
  );
  const onWorkerDestroyed = new Promise(resolve => {
    const emptyFn = () => {};
    const onDestroyed = ({ targetFront }) => {
      targetCommand.unwatchTargets([TYPES.WORKER], emptyFn, onDestroyed);
      resolve(targetFront);
    };

    targetCommand.watchTargets([TYPES.WORKER], emptyFn, onDestroyed);
  });
  worker2.terminate();
  const workerTargetFront = await onWorkerDestroyed;
  ok(true, "onDestroyed was called when the worker was terminated");

  workerTargetFront.isTopLevel;
  ok(
    true,
    "isTopLevel can be called on the target front after onDestroyed was called"
  );

  workerTargetFront.name;
  ok(
    true,
    "name can be accessed on the target front after onDestroyed was called"
  );

  targetCommand.destroy();

  info("Unregister service workers so they don't appear in other tests.");
  await unregisterAllServiceWorkers(commands.client);

  await commands.destroy();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});

function sortFronts(f1, f2) {
  return f1.actorID < f2.actorID;
}
