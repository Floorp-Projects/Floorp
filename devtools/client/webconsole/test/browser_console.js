/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the basic features of the Browser Console, bug 587757.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html?" + Date.now();

const TEST_XHR_ERROR_URI = `http://example.com/404.html?${Date.now()}`;

"use strict";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);

  let opened = waitForConsole();

  let hud = HUDService.getBrowserConsole();
  ok(!hud, "browser console is not open");
  info("wait for the browser console to open with ctrl-shift-j");
  EventUtils.synthesizeKey("j", { accelKey: true, shiftKey: true }, window);

  hud = yield opened;
  ok(hud, "browser console opened");

  yield consoleOpened(hud);
});

function consoleOpened(hud) {
  hud.jsterm.clearOutput(true);

  expectUncaughtException();
  executeSoon(() => {
    foobarExceptionBug587757();
  });

  // Add a message from a chrome window.
  hud.iframeWindow.console.log("bug587757a");

  // Add a message from a content window.
  content.console.log("bug587757b");

  // Test eval.
  hud.jsterm.execute("document.location.href");

  // Check for network requests.
  let xhr = new XMLHttpRequest();
  xhr.onload = () => console.log("xhr loaded, status is: " + xhr.status);
  xhr.open("get", TEST_URI, true);
  xhr.send();

  // Check for xhr error.
  let xhrErr = new XMLHttpRequest();
  xhrErr.onload = () => {
    console.log("xhr error loaded, status is: " + xhrErr.status);
  };
  xhrErr.open("get", TEST_XHR_ERROR_URI, true);
  xhrErr.send();

  return waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "chrome window console.log() is displayed",
        text: "bug587757a",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "content window console.log() is displayed",
        text: "bug587757b",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        name: "jsterm eval result",
        text: "browser.xul",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_LOG,
      },
      {
        name: "exception message",
        text: "foobarExceptionBug587757",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
      },
      {
        name: "network message",
        text: "test-console.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_INFO,
        isXhr: true,
      },
      {
        name: "xhr error message",
        text: "404.html",
        category: CATEGORY_NETWORK,
        severity: SEVERITY_ERROR,
        isXhr: true,
      },
    ],
  });
}

function waitForConsole() {
  let deferred = promise.defer();

  Services.obs.addObserver(function observer(aSubject) {
    Services.obs.removeObserver(observer, "web-console-created");
    aSubject.QueryInterface(Ci.nsISupportsString);

    let hud = HUDService.getBrowserConsole();
    ok(hud, "browser console is open");
    is(aSubject.data, hud.hudId, "notification hudId is correct");

    executeSoon(() => deferred.resolve(hud));
  }, "web-console-created", false);

  return deferred.promise;
}
