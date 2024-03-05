/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1515290 - Ensure that DevToolsServer runs in its own compartment when debugging
// chrome context. If not, Debugger API's addGlobal will throw when trying to attach
// to chrome scripts as debugger actor's module and the chrome script will be in the same
// compartment. Debugger and debuggee can't be running in the same compartment.

const CHROME_PAGE =
  "chrome://mochitests/content/browser/devtools/client/framework/" +
  "test/test_chrome_page.html";

add_task(async function () {
  await testChromeTab();
  await testMainProcess();
});

// Test that Tab Target can debug chrome pages
async function testChromeTab() {
  const tab = await addTab(CHROME_PAGE);
  const browser = tab.linkedBrowser;
  ok(!browser.isRemoteBrowser, "chrome page is not remote");
  ok(
    browser.contentWindow.document.nodePrincipal.isSystemPrincipal,
    "chrome page is a privileged document"
  );

  const onThreadActorInstantiated = new Promise(resolve => {
    const observe = function (subject, topic) {
      if (topic === "devtools-thread-ready") {
        Services.obs.removeObserver(observe, "devtools-thread-ready");
        const threadActor = subject.wrappedJSObject;
        resolve(threadActor);
      }
    };
    Services.obs.addObserver(observe, "devtools-thread-ready");
  });

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const sources = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.SOURCE],
    {
      onAvailable(resources) {
        sources.push(...resources);
      },
    }
  );
  ok(
    sources.find(s => s.url == CHROME_PAGE),
    "The thread actor is able to attach to the chrome page and its sources"
  );

  const threadActor = await onThreadActorInstantiated;
  const serverGlobal = Cu.getGlobalForObject(threadActor);
  isnot(
    loader.id,
    serverGlobal.loader.id,
    "The actors are loaded in a distinct loader in order for the actors to use its very own compartment"
  );

  const onDedicatedLoaderDestroy = new Promise(resolve => {
    const observe = function (subject, topic) {
      if (topic === "devtools:loader:destroy") {
        Services.obs.removeObserver(observe, "devtools:loader:destroy");
        resolve();
      }
    };
    Services.obs.addObserver(observe, "devtools:loader:destroy");
  });

  await commands.destroy();

  // Wait for the dedicated loader used for DevToolsServer to be destroyed
  // in order to prevent leak reports on try
  await onDedicatedLoaderDestroy;
}

// Test that Main process Target can debug chrome scripts
async function testMainProcess() {
  const onThreadActorInstantiated = new Promise(resolve => {
    const observe = function (subject, topic) {
      if (topic === "devtools-thread-ready") {
        Services.obs.removeObserver(observe, "devtools-thread-ready");
        const threadActor = subject.wrappedJSObject;
        resolve(threadActor);
      }
    };
    Services.obs.addObserver(observe, "devtools-thread-ready");
  });

  const client = await CommandsFactory.spawnClientToDebugSystemPrincipal();
  const commands = await CommandsFactory.forMainProcess({ client });
  await commands.targetCommand.startListening();

  const sources = [];
  await commands.resourceCommand.watchResources(
    [commands.resourceCommand.TYPES.SOURCE],
    {
      onAvailable(resources) {
        sources.push(...resources);
      },
    }
  );
  ok(
    sources.find(
      s => s.url == "resource://devtools/client/framework/devtools.js"
    ),
    "The thread actor is able to attach to the chrome script, like client modules"
  );

  const threadActor = await onThreadActorInstantiated;
  const serverGlobal = Cu.getGlobalForObject(threadActor);
  isnot(
    loader.id,
    serverGlobal.loader.id,
    "The actors are loaded in a distinct loader in order for the actors to use its very own compartment"
  );

  // As this target is remote (i.e. isn't a local tab) calling Target.destroy won't close
  // the client. So do it manually here in order to ensure cleaning up the DevToolsServer
  // spawn for this main process actor.
  await commands.destroy();
}
