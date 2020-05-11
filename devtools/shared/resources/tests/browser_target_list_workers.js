/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API around workers

const { TargetList } = require("devtools/shared/resources/target-list");
const { TYPES } = TargetList;

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const CHROME_WORKER_URL = CHROME_URL_ROOT + "test_worker.js";
const WORKER_URL = URL_ROOT_SSL + "test_worker.js";
const SERVICE_WORKER_URL = URL_ROOT_SSL + "test_service_worker.js";

add_task(async function() {
  // Enabled fission's pref as the TargetList is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(FISSION_TEST_URL);

  await testBrowserWorkers(mainRoot);
  await testTabWorkers(mainRoot, tab);

  await client.close();
});

async function testBrowserWorkers(mainRoot) {
  info("Test TargetList against workers via the parent process target");

  // Instantiate a worker in the parent process
  // eslint-disable-next-line no-unused-vars
  const worker = new Worker(CHROME_WORKER_URL + "#simple-worker");
  // eslint-disable-next-line no-unused-vars
  const sharedWorker = new SharedWorker(CHROME_WORKER_URL + "#shared-worker");

  const targetDescriptor = await mainRoot.getMainProcess();
  const target = await targetDescriptor.getTarget();
  const targetList = new TargetList(mainRoot, target);
  await targetList.startListening();

  // Very naive sanity check against getAllTargets(workerType)
  info("Check that getAllTargets returned the expected targets");
  const workers = await targetList.getAllTargets(TYPES.WORKER);
  const hasWorker = workers.find(workerTarget => {
    return workerTarget.url == CHROME_WORKER_URL + "#simple-worker";
  });
  ok(hasWorker, "retrieve the target for the worker");

  const sharedWorkers = await targetList.getAllTargets(TYPES.SHARED_WORKER);
  const hasSharedWorker = sharedWorkers.find(workerTarget => {
    return workerTarget.url == CHROME_WORKER_URL + "#shared-worker";
  });
  ok(hasSharedWorker, "retrieve the target for the shared worker");

  const serviceWorkers = await targetList.getAllTargets(TYPES.SERVICE_WORKER);
  const hasServiceWorker = serviceWorkers.find(workerTarget => {
    return workerTarget.url == SERVICE_WORKER_URL;
  });
  ok(hasServiceWorker, "retrieve the target for the service worker");

  info(
    "Check that calling getAllTargets again return the same target instances"
  );
  const workers2 = await targetList.getAllTargets(TYPES.WORKER);
  const sharedWorkers2 = await targetList.getAllTargets(TYPES.SHARED_WORKER);
  const serviceWorkers2 = await targetList.getAllTargets(TYPES.SERVICE_WORKER);
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
  const onAvailable = async ({ type, targetFront, isTopLevel }) => {
    ok(
      type === TYPES.WORKER ||
        type === TYPES.SHARED_WORKER ||
        type === TYPES.SERVICE_WORKER,
      "We are only notified about worker targets"
    );
    ok(
      targetFront == target ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    targets.push(targetFront);
  };
  await targetList.watchTargets(
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

  targetList.unwatchTargets(
    [TYPES.WORKER, TYPES.SHARED_WORKER, TYPES.SERVICE_WORKER],
    onAvailable
  );

  // Create a new worker and see if the worker target is reported
  const onWorkerCreated = new Promise(resolve => {
    const onAvailable2 = async ({ type, targetFront, isTopLevel }) => {
      if (targets.includes(targetFront)) {
        return;
      }
      targetList.unwatchTargets([TYPES.WORKER], onAvailable2);
      resolve(targetFront);
    };
    targetList.watchTargets([TYPES.WORKER], onAvailable2);
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

  const workers3 = await targetList.getAllTargets(TYPES.WORKER);
  const hasWorker2 = workers3.find(
    ({ url }) => url == `${CHROME_WORKER_URL}#second`
  );
  ok(hasWorker2, "retrieve the target for tab via getAllTargets");

  targetList.stopListening();
}

async function testTabWorkers(mainRoot, tab) {
  info("Test TargetList against workers via a tab target");

  // Create a TargetList for a given test tab
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

  // Check that calling getAllTargets(workers) return the same target instances
  const workers = await targetList.getAllTargets(TYPES.WORKER);
  is(workers.length, 1, "retrieved the worker");
  is(workers[0].url, WORKER_URL, "The first worker is the page worker");

  // Assert that watchTargets will call the create callback for all existing workers
  const targets = [];
  const onAvailable = async ({ type, targetFront, isTopLevel }) => {
    is(type, TYPES.WORKER, "We are only notified about worker targets");
    ok(!isTopLevel, "The workers are never top level");
    targets.push(targetFront);
  };
  await targetList.watchTargets([TYPES.WORKER], onAvailable);
  is(targets.length, 1, "retrieved just the worker");
  is(targets[0], workers[0], "Got the exact same target front");
  targetList.unwatchTargets([TYPES.WORKER], onAvailable);

  targetList.stopListening();

  BrowserTestUtils.removeTab(tab);
}

function sortFronts(f1, f2) {
  return f1.actorID < f2.actorID;
}
