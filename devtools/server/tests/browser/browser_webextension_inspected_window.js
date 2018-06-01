/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  WebExtensionInspectedWindowFront
} = require("devtools/shared/fronts/webextension-inspected-window");

const TEST_RELOAD_URL = `${MAIN_DOMAIN}/inspectedwindow-reload-target.sjs`;

async function setup(pageUrl) {
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      // This is just an empty extension used to ensure that the caller extension uuid
      // actually exists.
    }
  });

  await extension.startup();

  const fakeExtCallerInfo = {
    url: WebExtensionPolicy.getByID(extension.id).getURL("fake-caller-script.js"),
    lineNumber: 1,
    addonId: extension.id,
  };

  await addTab(pageUrl);
  initDebuggerServer();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);

  const [, tabClient] = await client.attachTab(form.actor);

  const [, consoleClient] = await client.attachConsole(form.consoleActor, []);

  const inspectedWindowFront = new WebExtensionInspectedWindowFront(client, form);

  return {
    client, form,
    tabClient, consoleClient,
    inspectedWindowFront,
    extension, fakeExtCallerInfo,
  };
}

async function teardown({client, extension}) {
  await client.close();
  DebuggerServer.destroy();
  gBrowser.removeCurrentTab();
  await extension.unload();
}

function waitForNextTabNavigated(client) {
  return new Promise(resolve => {
    client.addListener("tabNavigated", function tabNavigatedListener(evt, pkt) {
      if (pkt.state == "stop" && !pkt.isFrameSwitching) {
        client.removeListener("tabNavigated", tabNavigatedListener);
        resolve();
      }
    });
  });
}

function consoleEvalJS(consoleClient, jsCode) {
  return new Promise(resolve => {
    consoleClient.evaluateJS(jsCode, resolve);
  });
}

// Script used as the injectedScript option in the inspectedWindow.reload tests.
function injectedScript() {
  if (!window.pageScriptExecutedFirst) {
    window.addEventListener("DOMContentLoaded", function() {
      if (document.querySelector("pre")) {
        document.querySelector("pre").textContent = "injected script executed first";
      }
    }, {once: true});
  }
}

// Script evaluated in the target tab, to collect the results of injectedScript
// evaluation in the inspectedWindow.reload tests.
function collectEvalResults() {
  const results = [];
  let iframeDoc = document;

  while (iframeDoc) {
    if (iframeDoc.querySelector("pre")) {
      results.push(iframeDoc.querySelector("pre").textContent);
    }
    const iframe = iframeDoc.querySelector("iframe");
    iframeDoc = iframe ? iframe.contentDocument : null;
  }
  return JSON.stringify(results);
}

add_task(async function test_successfull_inspectedWindowEval_result() {
  const {
    client, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(MAIN_DOMAIN);

  const result = await inspectedWindowFront.eval(fakeExtCallerInfo,
                                                 "window.location", {});

  ok(result.value, "Got a result from inspectedWindow eval");
  is(result.value.href, MAIN_DOMAIN,
     "Got the expected window.location.href property value");
  is(result.value.protocol, "http:",
     "Got the expected window.location.protocol property value");

  await teardown({client, extension});
});

add_task(async function test_successfull_inspectedWindowEval_resultAsGrip() {
  const {
    client, inspectedWindowFront, form,
    extension, fakeExtCallerInfo,
  } = await setup(MAIN_DOMAIN);

  let result = await inspectedWindowFront.eval(fakeExtCallerInfo, "window", {
    evalResultAsGrip: true,
    toolboxConsoleActorID: form.consoleActor
  });

  ok(result.valueGrip, "Got a result from inspectedWindow eval");
  ok(result.valueGrip.actor, "Got a object actor as expected");
  is(result.valueGrip.type, "object", "Got a value grip of type object");
  is(result.valueGrip.class, "Window", "Got a value grip which is instanceof Location");

  // Test invalid evalResultAsGrip request.
  result = await inspectedWindowFront.eval(
    fakeExtCallerInfo, "window", {evalResultAsGrip: true}
  );

  ok(!result.value && !result.valueGrip,
     "Got a null result from the invalid inspectedWindow eval call");
  ok(result.exceptionInfo.isError, "Got an API Error result from inspectedWindow eval");
  ok(!result.exceptionInfo.isException, "An error isException is false as expected");
  is(result.exceptionInfo.code, "E_PROTOCOLERROR",
     "Got the expected 'code' property in the error result");
  is(result.exceptionInfo.description, "Inspector protocol error: %s - %s",
     "Got the expected 'description' property in the error result");
  is(result.exceptionInfo.details.length, 2,
     "The 'details' array property should contains 1 element");
  is(result.exceptionInfo.details[0],
     "Unexpected invalid sidebar panel expression request",
     "Got the expected content in the error results's details");
  is(result.exceptionInfo.details[1],
     "missing toolboxConsoleActorID",
     "Got the expected content in the error results's details");

  await teardown({client, extension});
});

add_task(async function test_error_inspectedWindowEval_result() {
  const {
    client, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(MAIN_DOMAIN);

  const result = await inspectedWindowFront.eval(fakeExtCallerInfo, "window", {});

  ok(!result.value, "Got a null result from inspectedWindow eval");
  ok(result.exceptionInfo.isError, "Got an API Error result from inspectedWindow eval");
  ok(!result.exceptionInfo.isException, "An error isException is false as expected");
  is(result.exceptionInfo.code, "E_PROTOCOLERROR",
     "Got the expected 'code' property in the error result");
  is(result.exceptionInfo.description, "Inspector protocol error: %s",
     "Got the expected 'description' property in the error result");
  is(result.exceptionInfo.details.length, 1,
     "The 'details' array property should contains 1 element");
  ok(result.exceptionInfo.details[0].includes("cyclic object value"),
     "Got the expected content in the error results's details");

  await teardown({client, extension});
});

add_task(async function test_system_principal_denied_error_inspectedWindowEval_result() {
  const {
    client, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup("about:addons");

  const result = await inspectedWindowFront.eval(fakeExtCallerInfo, "window", {});

  ok(!result.value, "Got a null result from inspectedWindow eval");
  ok(result.exceptionInfo.isError,
     "Got an API Error result from inspectedWindow eval on a system principal page");
  is(result.exceptionInfo.code, "E_PROTOCOLERROR",
     "Got the expected 'code' property in the error result");
  is(result.exceptionInfo.description, "Inspector protocol error: %s",
     "Got the expected 'description' property in the error result");
  is(result.exceptionInfo.details.length, 1,
     "The 'details' array property should contains 1 element");
  is(result.exceptionInfo.details[0],
     "This target has a system principal. inspectedWindow.eval denied.",
     "Got the expected content in the error results's details");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowEval_result() {
  const {
    client, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(MAIN_DOMAIN);

  const result = await inspectedWindowFront.eval(
    fakeExtCallerInfo, "throw Error('fake eval error');", {});

  ok(result.exceptionInfo.isException, "Got an exception as expected");
  ok(!result.value, "Got an undefined eval value");
  ok(!result.exceptionInfo.isError, "An exception should not be isError=true");
  ok(result.exceptionInfo.value.includes("Error: fake eval error"),
     "Got the expected exception message");

  const expectedCallerInfo =
    `called from ${fakeExtCallerInfo.url}:${fakeExtCallerInfo.lineNumber}`;
  ok(result.exceptionInfo.value.includes(expectedCallerInfo),
     "Got the expected caller info in the exception message");

  const expectedStack = `eval code:1:7`;
  ok(result.exceptionInfo.value.includes(expectedStack),
     "Got the expected stack trace in the exception message");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowReload() {
  const {
    client, consoleClient, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(`${TEST_RELOAD_URL}?test=cache`);

  // Test reload with bypassCache=false.

  const waitForNoBypassCacheReload = waitForNextTabNavigated(client);
  const reloadResult = await inspectedWindowFront.reload(fakeExtCallerInfo,
                                                         {ignoreCache: false});

  ok(!reloadResult, "Got the expected undefined result from inspectedWindow reload");

  await waitForNoBypassCacheReload;

  const noBypassCacheEval = await consoleEvalJS(consoleClient,
                                                "document.body.textContent");

  is(noBypassCacheEval.result, "empty cache headers",
     "Got the expected result with reload forceBypassCache=false");

  // Test reload with bypassCache=true.

  const waitForForceBypassCacheReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo, {ignoreCache: true});

  await waitForForceBypassCacheReload;

  const forceBypassCacheEval = await consoleEvalJS(consoleClient,
                                                   "document.body.textContent");

  is(forceBypassCacheEval.result, "no-cache:no-cache",
     "Got the expected result with reload forceBypassCache=true");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowReload_customUserAgent() {
  const {
    client, consoleClient, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(`${TEST_RELOAD_URL}?test=user-agent`);

  // Test reload with custom userAgent.

  const waitForCustomUserAgentReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo,
                                    {userAgent: "Customized User Agent"});

  await waitForCustomUserAgentReload;

  const customUserAgentEval = await consoleEvalJS(consoleClient,
                                                  "document.body.textContent");

  is(customUserAgentEval.result, "Customized User Agent",
     "Got the expected result on reload with a customized userAgent");

  // Test reload with no custom userAgent.

  const waitForNoCustomUserAgentReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo, {});

  await waitForNoCustomUserAgentReload;

  const noCustomUserAgentEval = await consoleEvalJS(consoleClient,
                                                    "document.body.textContent");

  is(noCustomUserAgentEval.result, window.navigator.userAgent,
     "Got the expected result with reload without a customized userAgent");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowReload_injectedScript() {
  const {
    client, consoleClient, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(`${TEST_RELOAD_URL}?test=injected-script&frames=3`);

  // Test reload with an injectedScript.

  const waitForInjectedScriptReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo,
                                    {injectedScript: `new ${injectedScript}`});
  await waitForInjectedScriptReload;

  const injectedScriptEval = await consoleEvalJS(consoleClient,
                                                 `(${collectEvalResults})()`);

  const expectedResult = (new Array(5)).fill("injected script executed first");

  SimpleTest.isDeeply(JSON.parse(injectedScriptEval.result), expectedResult,
     "Got the expected result on reload with an injected script");

  // Test reload without an injectedScript.

  const waitForNoInjectedScriptReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo, {});
  await waitForNoInjectedScriptReload;

  const noInjectedScriptEval = await consoleEvalJS(consoleClient,
                                                   `(${collectEvalResults})()`);

  const newExpectedResult = (new Array(5)).fill("injected script NOT executed");

  SimpleTest.isDeeply(JSON.parse(noInjectedScriptEval.result), newExpectedResult,
                      "Got the expected result on reload with no injected script");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowReload_multiple_calls() {
  const {
    client, consoleClient, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(`${TEST_RELOAD_URL}?test=user-agent`);

  // Test reload with custom userAgent three times (and then
  // check that only the first one has affected the page reload.

  const waitForCustomUserAgentReload = waitForNextTabNavigated(client);

  inspectedWindowFront.reload(fakeExtCallerInfo, {userAgent: "Customized User Agent 1"});
  inspectedWindowFront.reload(fakeExtCallerInfo, {userAgent: "Customized User Agent 2"});

  await waitForCustomUserAgentReload;

  const customUserAgentEval = await consoleEvalJS(consoleClient,
                                                  "document.body.textContent");

  is(customUserAgentEval.result, "Customized User Agent 1",
     "Got the expected result on reload with a customized userAgent");

  // Test reload with no custom userAgent.

  const waitForNoCustomUserAgentReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo, {});

  await waitForNoCustomUserAgentReload;

  const noCustomUserAgentEval = await consoleEvalJS(consoleClient,
                                                    "document.body.textContent");

  is(noCustomUserAgentEval.result, window.navigator.userAgent,
     "Got the expected result with reload without a customized userAgent");

  await teardown({client, extension});
});

add_task(async function test_exception_inspectedWindowReload_stopped() {
  const {
    client, consoleClient, inspectedWindowFront,
    extension, fakeExtCallerInfo,
  } = await setup(`${TEST_RELOAD_URL}?test=injected-script&frames=3`);

  // Test reload on a page that calls window.stop() immediately during the page loading

  const waitForPageLoad = waitForNextTabNavigated(client);
  await inspectedWindowFront.eval(fakeExtCallerInfo,
                                  "window.location += '&stop=windowStop'");

  info("Load a webpage that calls 'window.stop()' while is still loading");
  await waitForPageLoad;

  info("Starting a reload with an injectedScript");
  const waitForInjectedScriptReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo,
                                    {injectedScript: `new ${injectedScript}`});
  await waitForInjectedScriptReload;

  const injectedScriptEval = await consoleEvalJS(consoleClient,
                                                 `(${collectEvalResults})()`);

  // The page should have stopped during the reload and only one injected script
  // is expected.
  const expectedResult = (new Array(1)).fill("injected script executed first");

  SimpleTest.isDeeply(JSON.parse(injectedScriptEval.result), expectedResult,
     "The injected script has been executed on the 'stopped' page reload");

  // Reload again with no options.

  info("Reload the tab again without any reload options");
  const waitForNoInjectedScriptReload = waitForNextTabNavigated(client);
  await inspectedWindowFront.reload(fakeExtCallerInfo, {});
  await waitForNoInjectedScriptReload;

  const noInjectedScriptEval = await consoleEvalJS(consoleClient,
                                                   `(${collectEvalResults})()`);

  // The page should have stopped during the reload and no injected script should
  // have been executed during this second reload (or it would mean that the previous
  // customized reload was still pending and has wrongly affected the second reload)
  const newExpectedResult = (new Array(1)).fill("injected script NOT executed");

  SimpleTest.isDeeply(
    JSON.parse(noInjectedScriptEval.result), newExpectedResult,
    "No injectedScript should have been evaluated during the second reload"
  );

  await teardown({client, extension});
});

// TODO: check eval with $0 binding once implemented (Bug 1300590)
