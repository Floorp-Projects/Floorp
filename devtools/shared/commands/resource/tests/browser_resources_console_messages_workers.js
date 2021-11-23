/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CONSOLE_MESSAGE in workers

const FISSION_TEST_URL = URL_ROOT_SSL + "fission_document_workers.html";
const WORKER_FILE = "test_worker.js";
const IFRAME_FILE = `${URL_ROOT_ORG_SSL}fission_iframe_workers.html`;

add_task(async function() {
  // Set the following pref to false as it's the one that enables direct connection
  // to the worker targets
  await pushPref("dom.worker.console.dispatch_events_to_main_thread", false);

  const tab = await addTab(FISSION_TEST_URL);

  const {
    client,
    resourceCommand,
    targetCommand,
  } = await initResourceCommand(tab, { listenForWorkers: true });

  info("Wait for the workers (from the main page and the iframe) to be ready");
  const targets = [];
  await new Promise(resolve => {
    const onAvailable = async ({ targetFront }) => {
      targets.push(targetFront);
      if (targets.length === 2) {
        resolve();
      }
    };
    targetCommand.watchTargets([targetCommand.TYPES.WORKER], onAvailable);
  });

  // The worker logs a message right when it starts, containing its location, so we can
  // assert that we get the logs from the worker spawned in the content page and from the
  // worker spawned in the iframe.
  info("Check that we receive the cached messages");

  const resources = [];
  const onAvailable = innerResources => {
    for (const resource of innerResources) {
      // Ignore resources from non worker targets
      if (!resource.targetFront.isWorkerTarget) {
        continue;
      }

      resources.push(resource);
    }
  };

  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable,
    }
  );

  is(resources.length, 2, "Got the expected number of existing messages");
  const startLogFromWorkerInMainPage = resources.find(
    ({ message }) =>
      message.arguments[1] === `${URL_ROOT_SSL}${WORKER_FILE}#simple-worker`
  );
  const startLogFromWorkerInIframe = resources.find(
    ({ message }) =>
      message.arguments[1] ===
      `${URL_ROOT_ORG_SSL}${WORKER_FILE}#simple-worker-in-iframe`
  );

  checkStartWorkerLogMessage(startLogFromWorkerInMainPage, {
    expectedUrl: `${URL_ROOT_SSL}${WORKER_FILE}#simple-worker`,
    isAlreadyExistingResource: true,
  });
  checkStartWorkerLogMessage(startLogFromWorkerInIframe, {
    expectedUrl: `${URL_ROOT_ORG_SSL}${WORKER_FILE}#simple-worker-in-iframe`,
    isAlreadyExistingResource: true,
  });
  let messageCount = resources.length;

  info(
    "Now log messages *after* the call to ResourceCommand.watchResources and after having received all existing messages"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.wrappedJSObject.logMessageInWorker("live message from main page");

    const iframe = content.document.querySelector("iframe");
    SpecialPowers.spawn(iframe, [], () => {
      content.wrappedJSObject.logMessageInWorker("live message from iframe");
    });
  });

  // Wait until the 2 new logs are available
  await waitUntil(() => resources.length === messageCount + 2);
  const liveMessageFromWorkerInMainPage = resources.find(
    ({ message }) => message.arguments[1] === "live message from main page"
  );
  const liveMessageFromWorkerInIframe = resources.find(
    ({ message }) => message.arguments[1] === "live message from iframe"
  );

  checkLogInWorkerMessage(
    liveMessageFromWorkerInMainPage,
    "live message from main page"
  );

  checkLogInWorkerMessage(
    liveMessageFromWorkerInIframe,
    "live message from iframe"
  );

  // update the current number of resources received
  messageCount = resources.length;

  info("Now spawn new workers and log messages in main page and iframe");
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [WORKER_FILE],
    async workerUrl => {
      const spawnedWorker = new content.Worker(`${workerUrl}#spawned-worker`);
      spawnedWorker.postMessage({
        type: "log-in-worker",
        message: "live message in spawned worker from main page",
      });

      const iframe = content.document.querySelector("iframe");
      SpecialPowers.spawn(iframe, [workerUrl], async innerWorkerUrl => {
        const spawnedWorkerInIframe = new content.Worker(
          `${innerWorkerUrl}#spawned-worker-in-iframe`
        );
        spawnedWorkerInIframe.postMessage({
          type: "log-in-worker",
          message: "live message in spawned worker from iframe",
        });
      });
    }
  );

  info(
    "Wait until the 4 new logs are available (the ones logged at worker creation + the ones from postMessage"
  );
  await waitUntil(
    () => resources.length === messageCount + 4,
    `Couldn't get the expected number of resources (expected ${messageCount +
      4}, got ${resources.length})`
  );
  const startLogFromSpawnedWorkerInMainPage = resources.find(
    ({ message }) =>
      message.arguments[1] === `${URL_ROOT_SSL}${WORKER_FILE}#spawned-worker`
  );
  const startLogFromSpawnedWorkerInIframe = resources.find(
    ({ message }) =>
      message.arguments[1] ===
      `${URL_ROOT_ORG_SSL}${WORKER_FILE}#spawned-worker-in-iframe`
  );
  const liveMessageFromSpawnedWorkerInMainPage = resources.find(
    ({ message }) =>
      message.arguments[1] === "live message in spawned worker from main page"
  );
  const liveMessageFromSpawnedWorkerInIframe = resources.find(
    ({ message }) =>
      message.arguments[1] === "live message in spawned worker from iframe"
  );

  checkStartWorkerLogMessage(startLogFromSpawnedWorkerInMainPage, {
    expectedUrl: `${URL_ROOT_SSL}${WORKER_FILE}#spawned-worker`,
  });
  checkStartWorkerLogMessage(startLogFromSpawnedWorkerInIframe, {
    expectedUrl: `${URL_ROOT_ORG_SSL}${WORKER_FILE}#spawned-worker-in-iframe`,
  });
  checkLogInWorkerMessage(
    liveMessageFromSpawnedWorkerInMainPage,
    "live message in spawned worker from main page"
  );
  checkLogInWorkerMessage(
    liveMessageFromSpawnedWorkerInIframe,
    "live message in spawned worker from iframe"
  );
  // update the current number of resources received
  messageCount = resources.length;

  info(
    "Add a remote iframe on the same origin we already have an iframe and check we get the messages"
  );
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [IFRAME_FILE],
    async iframeUrl => {
      const iframe = content.document.createElement("iframe");
      iframe.src = `${iframeUrl}?hashSuffix=in-second-iframe`;
      content.document.body.append(iframe);
    }
  );

  info("Wait until the new log is available");
  await waitUntil(
    () => resources.length === messageCount + 1,
    `Couldn't get the expected number of resources (expected ${messageCount +
      1}, got ${resources.length})`
  );
  const startLogFromWorkerInSecondIframe = resources[resources.length - 1];
  checkStartWorkerLogMessage(startLogFromWorkerInSecondIframe, {
    expectedUrl: `${URL_ROOT_ORG_SSL}${WORKER_FILE}#simple-worker-in-second-iframe`,
  });

  targetCommand.destroy();
  await client.close();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});

function checkStartWorkerLogMessage(
  resource,
  { expectedUrl, isAlreadyExistingResource = false }
) {
  const { message } = resource;
  const [firstArg, secondArg, thirdArg] = message.arguments;
  is(firstArg, "[WORKER] started", "Got the expected first argument");
  is(secondArg, expectedUrl, "expected url was logged");
  is(
    thirdArg?._grip?.class,
    "DedicatedWorkerGlobalScope",
    "the global scope was logged as expected"
  );
  is(
    resource.isAlreadyExistingResource,
    isAlreadyExistingResource,
    "Resource has expected value for isAlreadyExistingResource"
  );
}

function checkLogInWorkerMessage(resource, expectedMessage) {
  const { message } = resource;
  const [firstArg, secondArg, thirdArg] = message.arguments;
  is(firstArg, "[WORKER]", "Got the expected first argument");
  is(secondArg, expectedMessage, "expected message was logged");
  is(
    thirdArg?._grip?.class,
    "MessageEvent",
    "the message event object was logged as expected"
  );
  is(
    resource.isAlreadyExistingResource,
    false,
    "Resource has expected value for isAlreadyExistingResource"
  );
}
