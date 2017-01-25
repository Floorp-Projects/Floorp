/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  WebExtensionInspectedWindowFront
} = require("devtools/shared/fronts/webextension-inspected-window");

const TEST_RELOAD_URL = `${MAIN_DOMAIN}/inspectedwindow-reload-target.sjs`;

const FAKE_CALLER_INFO = {
  url: "moz-extension://fake-webextension-uuid/fake-caller-script.js",
  lineNumber: 1,
  addonId: "fake-webextension-uuid",
};

function* setup(pageUrl) {
  yield addTab(pageUrl);
  initDebuggerServer();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = yield connectDebuggerClient(client);

  const [, tabClient] = yield client.attachTab(form.actor);

  const [, consoleClient] = yield client.attachConsole(form.consoleActor, []);

  const inspectedWindowFront = new WebExtensionInspectedWindowFront(client, form);

  return {
    client, form,
    tabClient, consoleClient,
    inspectedWindowFront,
  };
}

function* teardown({client}) {
  yield client.close();
  DebuggerServer.destroy();
  gBrowser.removeCurrentTab();
}

function waitForNextTabNavigated(client) {
  return new Promise(resolve => {
    client.addListener("tabNavigated", function tabNavigatedListener(evt, pkt) {
      if (pkt.state == "stop" && pkt.isFrameSwitching == false) {
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
    window.addEventListener("DOMContentLoaded", function () {
      if (document.querySelector("pre")) {
        document.querySelector("pre").textContent = "injected script executed first";
      }
    }, {once: true});
  }
}

// Script evaluated in the target tab, to collect the results of injectedScript
// evaluation in the inspectedWindow.reload tests.
function collectEvalResults() {
  let results = [];
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

add_task(function* test_successfull_inspectedWindowEval_result() {
  const {client, inspectedWindowFront} = yield setup(MAIN_DOMAIN);
  const result = yield inspectedWindowFront.eval(FAKE_CALLER_INFO, "window.location", {});

  ok(result.value, "Got a result from inspectedWindow eval");
  is(result.value.href, MAIN_DOMAIN,
     "Got the expected window.location.href property value");
  is(result.value.protocol, "http:",
     "Got the expected window.location.protocol property value");

  yield teardown({client});
});

add_task(function* test_error_inspectedWindowEval_result() {
  const {client, inspectedWindowFront} = yield setup(MAIN_DOMAIN);
  const result = yield inspectedWindowFront.eval(FAKE_CALLER_INFO, "window", {});

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

  yield teardown({client});
});

add_task(function* test_system_principal_denied_error_inspectedWindowEval_result() {
  const {client, inspectedWindowFront} = yield setup("about:addons");
  const result = yield inspectedWindowFront.eval(FAKE_CALLER_INFO, "window", {});

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

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowEval_result() {
  const {client, inspectedWindowFront} = yield setup(MAIN_DOMAIN);
  const result = yield inspectedWindowFront.eval(
    FAKE_CALLER_INFO, "throw Error('fake eval error');", {});

  ok(result.exceptionInfo.isException, "Got an exception as expected");
  ok(!result.value, "Got an undefined eval value");
  ok(!result.exceptionInfo.isError, "An exception should not be isError=true");
  ok(result.exceptionInfo.value.includes("Error: fake eval error"),
     "Got the expected exception message");

  const expectedCallerInfo =
    `called from ${FAKE_CALLER_INFO.url}:${FAKE_CALLER_INFO.lineNumber}`;
  ok(result.exceptionInfo.value.includes(expectedCallerInfo),
     "Got the expected caller info in the exception message");

  const expectedStack = `eval code:1:7`;
  ok(result.exceptionInfo.value.includes(expectedStack),
     "Got the expected stack trace in the exception message");

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowReload() {
  const {
    client, consoleClient, inspectedWindowFront,
  } = yield setup(`${TEST_RELOAD_URL}?test=cache`);

  // Test reload with bypassCache=false.

  const waitForNoBypassCacheReload = waitForNextTabNavigated(client);
  const reloadResult = yield inspectedWindowFront.reload(FAKE_CALLER_INFO,
                                                         {ignoreCache: false});

  ok(!reloadResult, "Got the expected undefined result from inspectedWindow reload");

  yield waitForNoBypassCacheReload;

  const noBypassCacheEval = yield consoleEvalJS(consoleClient,
                                                "document.body.textContent");

  is(noBypassCacheEval.result, "empty cache headers",
     "Got the expected result with reload forceBypassCache=false");

  // Test reload with bypassCache=true.

  const waitForForceBypassCacheReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO, {ignoreCache: true});

  yield waitForForceBypassCacheReload;

  const forceBypassCacheEval = yield consoleEvalJS(consoleClient,
                                                   "document.body.textContent");

  is(forceBypassCacheEval.result, "no-cache:no-cache",
     "Got the expected result with reload forceBypassCache=true");

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowReload_customUserAgent() {
  const {
    client, consoleClient, inspectedWindowFront,
  } = yield setup(`${TEST_RELOAD_URL}?test=user-agent`);

  // Test reload with custom userAgent.

  const waitForCustomUserAgentReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO,
                                    {userAgent: "Customized User Agent"});

  yield waitForCustomUserAgentReload;

  const customUserAgentEval = yield consoleEvalJS(consoleClient,
                                                  "document.body.textContent");

  is(customUserAgentEval.result, "Customized User Agent",
     "Got the expected result on reload with a customized userAgent");

  // Test reload with no custom userAgent.

  const waitForNoCustomUserAgentReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO, {});

  yield waitForNoCustomUserAgentReload;

  const noCustomUserAgentEval = yield consoleEvalJS(consoleClient,
                                                    "document.body.textContent");

  is(noCustomUserAgentEval.result, window.navigator.userAgent,
     "Got the expected result with reload without a customized userAgent");

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowReload_injectedScript() {
  const {
    client, consoleClient, inspectedWindowFront,
  } = yield setup(`${TEST_RELOAD_URL}?test=injected-script&frames=3`);

  // Test reload with an injectedScript.

  const waitForInjectedScriptReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO,
                                    {injectedScript: `new ${injectedScript}`});
  yield waitForInjectedScriptReload;

  const injectedScriptEval = yield consoleEvalJS(consoleClient,
                                                 `(${collectEvalResults})()`);

  const expectedResult = (new Array(4)).fill("injected script executed first");

  SimpleTest.isDeeply(JSON.parse(injectedScriptEval.result), expectedResult,
     "Got the expected result on reload with an injected script");

  // Test reload without an injectedScript.

  const waitForNoInjectedScriptReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO, {});
  yield waitForNoInjectedScriptReload;

  const noInjectedScriptEval = yield consoleEvalJS(consoleClient,
                                                   `(${collectEvalResults})()`);

  const newExpectedResult = (new Array(4)).fill("injected script NOT executed");

  SimpleTest.isDeeply(JSON.parse(noInjectedScriptEval.result), newExpectedResult,
                      "Got the expected result on reload with no injected script");

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowReload_multiple_calls() {
  const {
    client, consoleClient, inspectedWindowFront,
  } = yield setup(`${TEST_RELOAD_URL}?test=user-agent`);

  // Test reload with custom userAgent three times (and then
  // check that only the first one has affected the page reload.

  const waitForCustomUserAgentReload = waitForNextTabNavigated(client);

  inspectedWindowFront.reload(FAKE_CALLER_INFO, {userAgent: "Customized User Agent 1"});
  inspectedWindowFront.reload(FAKE_CALLER_INFO, {userAgent: "Customized User Agent 2"});

  yield waitForCustomUserAgentReload;

  const customUserAgentEval = yield consoleEvalJS(consoleClient,
                                                  "document.body.textContent");

  is(customUserAgentEval.result, "Customized User Agent 1",
     "Got the expected result on reload with a customized userAgent");

  // Test reload with no custom userAgent.

  const waitForNoCustomUserAgentReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO, {});

  yield waitForNoCustomUserAgentReload;

  const noCustomUserAgentEval = yield consoleEvalJS(consoleClient,
                                                    "document.body.textContent");

  is(noCustomUserAgentEval.result, window.navigator.userAgent,
     "Got the expected result with reload without a customized userAgent");

  yield teardown({client});
});

add_task(function* test_exception_inspectedWindowReload_stopped() {
  const {
    client, consoleClient, inspectedWindowFront,
  } = yield setup(`${TEST_RELOAD_URL}?test=injected-script&frames=3`);

  // Test reload on a page that calls window.stop() immediately during the page loading

  const waitForPageLoad = waitForNextTabNavigated(client);
  yield inspectedWindowFront.eval(FAKE_CALLER_INFO,
                                  "window.location += '&stop=windowStop'");

  info("Load a webpage that calls 'window.stop()' while is still loading");
  yield waitForPageLoad;

  info("Starting a reload with an injectedScript");
  const waitForInjectedScriptReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO,
                                    {injectedScript: `new ${injectedScript}`});
  yield waitForInjectedScriptReload;

  const injectedScriptEval = yield consoleEvalJS(consoleClient,
                                                 `(${collectEvalResults})()`);

  // The page should have stopped during the reload and only one injected script
  // is expected.
  const expectedResult = (new Array(1)).fill("injected script executed first");

  SimpleTest.isDeeply(JSON.parse(injectedScriptEval.result), expectedResult,
     "The injected script has been executed on the 'stopped' page reload");

  // Reload again with no options.

  info("Reload the tab again without any reload options");
  const waitForNoInjectedScriptReload = waitForNextTabNavigated(client);
  yield inspectedWindowFront.reload(FAKE_CALLER_INFO, {});
  yield waitForNoInjectedScriptReload;

  const noInjectedScriptEval = yield consoleEvalJS(consoleClient,
                                                   `(${collectEvalResults})()`);

  // The page should have stopped during the reload and no injected script should
  // have been executed during this second reload (or it would mean that the previous
  // customized reload was still pending and has wrongly affected the second reload)
  const newExpectedResult = (new Array(1)).fill("injected script NOT executed");

  SimpleTest.isDeeply(
    JSON.parse(noInjectedScriptEval.result), newExpectedResult,
    "No injectedScript should have been evaluated during the second reload"
  );

  yield teardown({client});
});

// TODO: check eval with $0 binding once implemented (Bug 1300590)
