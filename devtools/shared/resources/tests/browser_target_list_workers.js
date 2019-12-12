/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetList API around workers

const { TargetList } = require("devtools/shared/resources/target-list");

const FISSION_TEST_URL = URL_ROOT + "fission_document.html";
const CHROME_WORKER_URL = CHROME_URL_ROOT + "test_worker.js";
const WORKER_URL = URL_ROOT + "test_worker.js";

add_task(async function() {
  // Enabled fission's pref as the TargetList is almost disabled without it
  await pushPref("devtools.browsertoolbox.fission", true);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  await testBrowserWorkers(mainRoot);
  await testTabWorkers(mainRoot);

  await client.close();
});

async function testBrowserWorkers(mainRoot) {
  info("Test TargetList against workers via the parent process target");

  // Instantiate a worker in the parent process
  // eslint-disable-next-line no-unused-vars
  const worker = new Worker(CHROME_WORKER_URL);

  const target = await mainRoot.getMainProcess();
  const targetList = new TargetList(mainRoot, target);
  await targetList.startListening([TargetList.TYPES.WORKER]);

  // Very naive sanity check against getAllTargets(worker)
  const workers = await targetList.getAllTargets(TargetList.TYPES.WORKER);
  const hasWorker = workers.find(workerTarget => {
    return workerTarget.url == CHROME_WORKER_URL;
  });
  ok(hasWorker, "retrieve the target for the worker");

  // Check that calling getAllTargets(worker) return the same target instances
  const workers2 = await targetList.getAllTargets(TargetList.TYPES.WORKER);
  is(workers2.length, workers.length, "retrieved the same number of workers");

  function sortFronts(f1, f2) {
    return f1.actorID < f2.actorID;
  }
  workers.sort(sortFronts);
  workers2.sort(sortFronts);
  for (let i = 0; i < workers.length; i++) {
    is(workers[i], workers2[i], `worker ${i} targets are the same`);
  }

  // Assert that watchTargets will call the create callback for all existing workers
  const targets = [];
  const onAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.WORKER,
      "We are only notified about worker targets"
    );
    ok(
      targetFront == target ? isTopLevel : !isTopLevel,
      "isTopLevel argument is correct"
    );
    targets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.WORKER], onAvailable);
  is(
    targets.length,
    workers.length,
    "retrieved the same number of workers via watchTargets"
  );

  workers.sort(sortFronts);
  targets.sort(sortFronts);
  for (let i = 0; i < workers.length; i++) {
    is(
      workers[i],
      targets[i],
      `worker ${i} targets are the same via watchTargets`
    );
  }
  targetList.unwatchTargets([TargetList.TYPES.WORKER], onAvailable);

  // Create a new worker and see if the worker target is reported
  const onWorkerCreated = new Promise(resolve => {
    const onAvailable2 = ({ type, targetFront, isTopLevel }) => {
      if (targets.includes(targetFront)) {
        return;
      }
      targetList.unwatchTargets([TargetList.TYPES.WORKER], onAvailable2);
      resolve(targetFront);
    };
    targetList.watchTargets([TargetList.TYPES.WORKER], onAvailable2);
  });
  // eslint-disable-next-line no-unused-vars
  const worker2 = new Worker(CHROME_WORKER_URL + "#second");
  const workerTarget = await onWorkerCreated;

  is(
    workerTarget.url,
    CHROME_WORKER_URL + "#second",
    "This worker target is about the new worker"
  );

  const workers3 = await targetList.getAllTargets(TargetList.TYPES.WORKER);
  const hasWorker2 = workers3.find(
    worderTarget => workerTarget.url == CHROME_WORKER_URL + "#second"
  );
  ok(hasWorker2, "retrieve the target for tab via getAllTargets");

  targetList.stopListening([TargetList.TYPES.WORKER]);
}

async function testTabWorkers(mainRoot) {
  info("Test TargetList against workers via a tab target");

  // Create a TargetList for a given test tab
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  const tab = await addTab(FISSION_TEST_URL);
  const target = await mainRoot.getTab({ tab });

  // Ensure attaching the target as BrowsingContextTargetActor.listWorkers
  // assert that the target actor is attached.
  // It isn't clear if this assertion is meaningful?
  await target.attach();

  const targetList = new TargetList(mainRoot, target);

  // Workaround to allow listening for workers in the content toolbox
  // without the fission preferences
  targetList.listenForWorkers = true;

  await targetList.startListening([TargetList.TYPES.WORKER]);

  // Check that calling getAllTargets(workers) return the same target instances
  const workers = await targetList.getAllTargets(TargetList.TYPES.WORKER);
  is(workers.length, 1, "retrieved the worker");
  is(workers[0].url, WORKER_URL, "The first worker is the page worker");

  // Assert that watchTargets will call the create callback for all existing workers
  const targets = [];
  const onAvailable = ({ type, targetFront, isTopLevel }) => {
    is(
      type,
      TargetList.TYPES.WORKER,
      "We are only notified about worker targets"
    );
    ok(!isTopLevel, "The workers are never top level");
    targets.push(targetFront);
  };
  await targetList.watchTargets([TargetList.TYPES.WORKER], onAvailable);
  is(targets.length, 1, "retrieved just the worker");
  is(targets[0], workers[0], "Got the exact same target front");
  targetList.unwatchTargets([TargetList.TYPES.WORKER], onAvailable);

  targetList.stopListening([TargetList.TYPES.WORKER]);

  BrowserTestUtils.removeTab(tab);
}
