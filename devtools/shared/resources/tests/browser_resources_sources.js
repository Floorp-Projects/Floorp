/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around SOURCE.

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const TEST_URL = URL_ROOT_SSL + "sources.html";

add_task(async function() {
  const tab = await addTab(TEST_URL);

  const htmlRequest = await fetch(TEST_URL);
  const htmlContent = await htmlRequest.text();

  const { client, resourceWatcher, targetCommand } = await initResourceCommand(
    tab
  );

  // Force the target list to cover workers
  targetCommand.listenForWorkers = true;
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  const targets = [];
  await targetCommand.watchTargets(targetCommand.ALL_TYPES, async function({
    targetFront,
  }) {
    targets.push(targetFront);
  });
  is(targets.length, 3, "Got expected number of targets");

  info("Check already available resources");
  const availableResources = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.SOURCE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  const expectedExistingResources = [
    {
      description: "independent js file",
      sourceForm: {
        introductionType: "scriptElement",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/resources/tests/sources.js",
        url:
          "https://example.com/browser/devtools/shared/resources/tests/sources.js",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "/* eslint-disable */\nfunction scriptSource() {}\n",
      },
    },
    {
      description: "eval",
      sourceForm: {
        introductionType: "eval",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/resources/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "this.global = function evalFunction() {}",
      },
    },
    {
      description: "inline JS",
      sourceForm: {
        introductionType: "scriptElement",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/resources/tests/sources.html",
        url:
          "https://example.com/browser/devtools/shared/resources/tests/sources.html",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/html",
        source: htmlContent,
      },
    },
    {
      description: "worker script",
      sourceForm: {
        introductionType: undefined,
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/resources/tests/worker-sources.js",
        url:
          "https://example.com/browser/devtools/shared/resources/tests/worker-sources.js",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "/* eslint-disable */\nfunction workerSource() {}\n",
      },
    },
    {
      description: "service worker script",
      sourceForm: {
        introductionType: undefined,
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/resources/tests/service-worker-sources.js",
        url:
          "https://example.com/browser/devtools/shared/resources/tests/service-worker-sources.js",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "/* eslint-disable */\nfunction serviceWorkerSource() {}\n",
      },
    },
  ];
  await assertResources(availableResources, expectedExistingResources);

  await targetCommand.stopListening();
  await client.close();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});

async function assertResources(resources, expected) {
  is(
    resources.length,
    expected.length,
    "Length of existing resources is correct at initial"
  );
  for (let i = 0; i < resources.length; i++) {
    await assertResource(resources[i], expected);
  }
}

async function assertResource(source, expected) {
  info(`Checking resource "#${expected.description}"`);

  is(
    source.resourceType,
    ResourceCommand.TYPES.SOURCE,
    "Resource type is correct"
  );

  const threadFront = await source.targetFront.getFront("thread");
  // `source` is SourceActor's form()
  // so try to instantiate the related SourceFront:
  const sourceFront = threadFront.source(source);
  // then fetch source content
  const sourceContent = await sourceFront.source();

  // Order of sources is random, so we have to find the best expected resource.
  // The only unique attribute is the JS Source text content.
  const matchingExpected = expected.find(res => {
    return res.sourceContent.source == sourceContent.source;
  });
  ok(
    matchingExpected,
    `This source was expected with source content being "${sourceContent.source}"`
  );
  assertObject(
    sourceContent,
    matchingExpected.sourceContent,
    matchingExpected.description
  );

  assertObject(
    source,
    matchingExpected.sourceForm,
    matchingExpected.description
  );
}

function assertObject(object, expected, description) {
  for (const field in expected) {
    is(
      object[field],
      expected[field],
      `The value of ${field} is correct for "#${description}"`
    );
  }
}
