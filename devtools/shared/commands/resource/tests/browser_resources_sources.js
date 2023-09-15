/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around SOURCE.
//
// We cover each Spidermonkey Debugger Source's `introductionType`:
// https://searchfox.org/mozilla-central/rev/4c184ca81b28f1ccffbfd08f465709b95bcb4aa1/js/src/doc/Debugger/Debugger.Source.md#172-213
//
// And especially cover sources being GC-ed before DevTools are opened
// which are later recreated by `ThreadActor.resurrectSource`.

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

const TEST_URL = URL_ROOT_SSL + "sources.html";

const TEST_JS_URL = URL_ROOT_SSL + "sources.js";
const TEST_WORKER_URL = URL_ROOT_SSL + "worker-sources.js";
const TEST_SW_URL = URL_ROOT_SSL + "service-worker-sources.js";

async function getExpectedResources(ignoreUnresurrectedSources = false) {
  const htmlRequest = await fetch(TEST_URL);
  const htmlContent = await htmlRequest.text();

  // First list sources that aren't GC-ed, or that the thread actor is able to resurrect
  const expectedSources = [
    {
      description: "eval",
      sourceForm: {
        introductionType: "eval",
        sourceMapBaseURL: TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
        sourceMapBaseURL: TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "function anonymous(\n) {\nreturn 42;\n}",
      },
    },
    {
      description: "Event Handler",
      sourceForm: {
        introductionType: "eventHandler",
        sourceMapBaseURL: TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
        sourceMapBaseURL: TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
        sourceMapBaseURL: TEST_URL,
        url: TEST_URL,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: true,
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
        sourceMapBaseURL: TEST_WORKER_URL,
        url: TEST_WORKER_URL,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
        sourceMapBaseURL: TEST_SW_URL,
        url: TEST_SW_URL,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
        sourceMapBaseURL: TEST_JS_URL,
        url: TEST_JS_URL,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "/* eslint-disable */\nfunction scriptSource() {}\n",
      },
    },
  ];

  // Now list the sources that could be GC-ed for which the thread actor isn't able to resurrect.
  // This is the sources that we can't assert when we fetch sources after the page is already loaded.
  const unresurrectedSources = [
    {
      description: "DOM Timer",
      sourceForm: {
        introductionType: "domTimer",
        sourceMapBaseURL: TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
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
      description: "javascript URL",
      sourceForm: {
        introductionType: "javascriptURL",
        sourceMapBaseURL: isEveryFrameTargetEnabled()
          ? "about:blank"
          : TEST_URL,
        url: null,
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "666",
      },
    },
    {
      description: "srcdoc attribute on iframes #1",
      sourceForm: {
        introductionType: "scriptElement",
        // We do not assert url/sourceMapBaseURL as it includes the Debugger.Source.id
        // which is random
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "console.log('srcdoc')",
      },
    },
    {
      description: "srcdoc attribute on iframes #2",
      sourceForm: {
        introductionType: "scriptElement",
        // We do not assert url/sourceMapBaseURL as it includes the Debugger.Source.id
        // which is random
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "console.log('srcdoc 2')",
      },
    },
  ];

  if (ignoreUnresurrectedSources) {
    return expectedSources;
  }
  return expectedSources.concat(unresurrectedSources);
}

add_task(async function testSourcesOnload() {
  // Load an blank document first, in order to load the test page only once we already
  // started watching for sources
  const tab = await addTab("about:blank");

  const commands = await CommandsFactory.forTab(tab);
  const { targetCommand, resourceCommand } = commands;

  // Force the target list to cover workers and debug all the targets
  targetCommand.listenForWorkers = true;
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  info("Check already available resources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  const promiseLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    TEST_URL
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, TEST_URL);
  await promiseLoad;

  // Some sources may be created after the document is done loading (like eventHandler usecase)
  // so we may be received *after* watchResource resolved
  const expectedResources = await getExpectedResources();
  await waitFor(
    () => availableResources.length >= expectedResources.length,
    "Got all the sources"
  );

  await assertResources(availableResources, expectedResources);

  await commands.destroy();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});

add_task(async function testGarbagedCollectedSources() {
  info(
    "Assert SOURCES on an already loaded page with some sources that have been GC-ed"
  );
  const tab = await addTab(TEST_URL);

  info("Force some GC to free some sources");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    Cu.forceGC();
    Cu.forceCC();
  });

  const commands = await CommandsFactory.forTab(tab);
  const { targetCommand, resourceCommand } = commands;

  // Force the target list to cover workers and debug all the targets
  targetCommand.listenForWorkers = true;
  targetCommand.listenForServiceWorkers = true;
  await targetCommand.startListening();

  info("Check already available resources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: resources => availableResources.push(...resources),
  });

  // Some sources may be created after the document is done loading (like eventHandler usecase)
  // so we may be received *after* watchResource resolved
  const expectedResources = await getExpectedResources(true);
  await waitFor(
    () => availableResources.length >= expectedResources.length,
    "Got all the sources"
  );

  await assertResources(availableResources, expectedResources);

  await commands.destroy();

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // registrationPromise is set by the test page.
    const registration = await content.wrappedJSObject.registrationPromise;
    registration.unregister();
  });
});

/**
 * Assert that evaluating sources for a new global, in the parent process
 * using the shared system principal will spawn SOURCE resources.
 *
 * For this we use a special `commands` which replicate what browser console
 * and toolbox use.
 */
add_task(async function testParentProcessPrivilegedSources() {
  // Use a custom loader + server + client in order to spawn the server
  // in a distinct system compartment, so that it can see the system compartment
  // sandbox we are about to create in this test
  const client = await CommandsFactory.spawnClientToDebugSystemPrincipal();

  const commands = await CommandsFactory.forMainProcess({ client });
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  info("Check already available resources");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.SOURCE], {
    onAvailable: resources => availableResources.push(...resources),
  });
  ok(
    !!availableResources.length,
    "We get many sources reported from a multiprocess command"
  );

  // Clear the list of sources
  availableResources.length = 0;

  // Force the creation of a new privileged source
  const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  const sandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox("function foo() {}", sandbox, null, "http://foo.com");

  info("Wait for the sandbox source");
  await waitFor(() => {
    return availableResources.some(
      resource => resource.url == "http://foo.com/"
    );
  });

  const expectedResources = [
    {
      description: "privileged sandbox script",
      sourceForm: {
        introductionType: undefined,
        sourceMapBaseURL: "http://foo.com/",
        url: "http://foo.com/",
        isBlackBoxed: false,
        sourceMapURL: null,
        extensionName: null,
        isInlineSource: false,
      },
      sourceContent: {
        contentType: "text/javascript",
        source: "function foo() {}",
      },
    },
  ];
  const matchingResource = availableResources.filter(resource =>
    resource.url.includes("http://foo.com")
  );
  await assertResources(matchingResource, expectedResources);

  await commands.destroy();
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
