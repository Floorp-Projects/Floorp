/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around SOURCE.
//
// We cover each Spidermonkey Debugger Source's `introductionType`:
// https://searchfox.org/mozilla-central/rev/4c184ca81b28f1ccffbfd08f465709b95bcb4aa1/js/src/doc/Debugger/Debugger.Source.md#172-213

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const TEST_URL = URL_ROOT_SSL + "sources.html";

add_task(async function() {
  const tab = await addTab(TEST_URL);

  const htmlRequest = await fetch(TEST_URL);
  const htmlContent = await htmlRequest.text();

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  // Force the target list to cover workers
  targetCommand.listenForWorkers = true;
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  const targets = [];
  await targetCommand.watchTargets({
    types: targetCommand.ALL_TYPES,
    onAvailable: async function({ targetFront }) {
      targets.push(targetFront);
    },
  });
  if (isEveryFrameTargetEnabled()) {
    is(targets.length, 4, "Got expected number of targets");
  } else {
    is(
      targets.length,
      3,
      "Got expected number of targets (without fission, nor EFT)"
    );
  }

  info("Check already available resources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  const expectedExistingResources = [
    {
      description: "eval",
      sourceForm: {
        introductionType: "eval",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
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
      description: "new Function()",
      sourceForm: {
        introductionType: "Function",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "return 42;",
      },
    },
    {
      description: "DOM Timer",
      sourceForm: {
        introductionType: "domTimer",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        /* the domTimer is prefixed by many empty lines in order to be positioned at the same line
           as in the HTML file where setTimeout is called.
           This is probably done by SourceActor.actualText().
           So the array size here, should be updated to match the line number of setTimeout call */
        source: new Array(39).join("\n") + `console.log("timeout")`,
      },
    },
    {
      description: "Event Handler",
      sourceForm: {
        introductionType: "eventHandler",
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "console.log('link')",
      },
    },
    {
      description: "inline JS inserted at runtime",
      sourceForm: {
        introductionType: "scriptElement", // This is an injectedScript at SpiderMonkey level, but is translated into scriptElement by SourceActor.form()
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "console.log('inline-script')",
      },
    },
    {
      description: "inline JS",
      sourceForm: {
        introductionType: "scriptElement", // This is an inlineScript at SpiderMonkey level, but is translated into scriptElement by SourceActor.form()
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
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
          "https://example.com/browser/devtools/shared/commands/resource/tests/worker-sources.js",
        url:
          "https://example.com/browser/devtools/shared/commands/resource/tests/worker-sources.js",
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
          "https://example.com/browser/devtools/shared/commands/resource/tests/service-worker-sources.js",
        url:
          "https://example.com/browser/devtools/shared/commands/resource/tests/service-worker-sources.js",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "/* eslint-disable */\nfunction serviceWorkerSource() {}\n",
      },
    },
    {
      description: "independent js file",
      sourceForm: {
        introductionType: "scriptElement", // This is an srcScript at SpiderMonkey level, but is translated into scriptElement by SourceActor.form()
        sourceMapBaseURL:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.js",
        url:
          "https://example.com/browser/devtools/shared/commands/resource/tests/sources.js",
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
      description: "javascript URL",
      sourceForm: {
        introductionType: "javascriptURL",
        sourceMapBaseURL: isEveryFrameTargetEnabled()
          ? "about:blank"
          : "https://example.com/browser/devtools/shared/commands/resource/tests/sources.html",
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "666",
      },
    },
  ];

  // Some sources may be created after the document is done loading (like eventHandler usecase)
  // so we may be received *after* watchResource resolved
  await waitFor(
    () => availableResources.length >= expectedExistingResources.length,
    "Got all the sources"
  );

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
  info(`Found "#${matchingExpected.description}"`);
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
