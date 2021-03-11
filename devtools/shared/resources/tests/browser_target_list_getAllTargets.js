/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API getAllTargets.

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document.html";
const CHROME_WORKER_URL = CHROME_URL_ROOT + "test_worker.js";

add_task(async function() {
  // Enabled devtools.browsertoolbox.fission to listen to all target types.
  await pushPref("devtools.browsertoolbox.fission", true);

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  info("Setup the test page with workers of all types");
  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  const tab = await addTab(FISSION_TEST_URL);

  // Instantiate a worker in the parent process
  // eslint-disable-next-line no-unused-vars
  const worker = new Worker(CHROME_WORKER_URL + "#simple-worker");
  // eslint-disable-next-line no-unused-vars
  const sharedWorker = new SharedWorker(CHROME_WORKER_URL + "#shared-worker");

  info("Create a target list for the main process target");
  const targetDescriptor = await mainRoot.getMainProcess();
  const commands = await targetDescriptor.getCommands();
  const targetList = commands.targetCommand;
  const { TYPES } = targetList;
  await targetList.startListening();

  info("Check getAllTargets will throw when providing invalid arguments");
  Assert.throws(
    () => targetList.getAllTargets(),
    e => e.message === "getAllTargets expects a non-empty array of types"
  );

  Assert.throws(
    () => targetList.getAllTargets([]),
    e => e.message === "getAllTargets expects a non-empty array of types"
  );

  info("Check getAllTargets returns consistent results with several types");
  const workerTargets = targetList.getAllTargets([TYPES.WORKER]);
  const serviceWorkerTargets = targetList.getAllTargets([TYPES.SERVICE_WORKER]);
  const sharedWorkerTargets = targetList.getAllTargets([TYPES.SHARED_WORKER]);
  const processTargets = targetList.getAllTargets([TYPES.PROCESS]);
  const frameTargets = targetList.getAllTargets([TYPES.FRAME]);

  const allWorkerTargetsReference = [
    ...workerTargets,
    ...serviceWorkerTargets,
    ...sharedWorkerTargets,
  ];
  const allWorkerTargets = targetList.getAllTargets([
    TYPES.WORKER,
    TYPES.SERVICE_WORKER,
    TYPES.SHARED_WORKER,
  ]);

  is(
    allWorkerTargets.length,
    allWorkerTargetsReference.length,
    "getAllTargets([worker, service, shared]) returned the expected number of targets"
  );

  ok(
    allWorkerTargets.every(t => allWorkerTargetsReference.includes(t)),
    "getAllTargets([worker, service, shared]) returned the expected targets"
  );

  const allTargetsReference = [
    ...allWorkerTargets,
    ...processTargets,
    ...frameTargets,
  ];
  const allTargets = targetList.getAllTargets(targetList.ALL_TYPES);
  is(
    allTargets.length,
    allTargetsReference.length,
    "getAllTargets(ALL_TYPES) returned the expected number of targets"
  );

  ok(
    allTargets.every(t => allTargetsReference.includes(t)),
    "getAllTargets(ALL_TYPES) returned the expected targets"
  );

  targetList.destroy();

  // Wait for all the targets to be fully attached so we don't have pending requests.
  await waitForAllTargetsToBeAttached(targetList);

  await client.close();
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});
