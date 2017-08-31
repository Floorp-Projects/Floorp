/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL1 = MAIN_DOMAIN + "navigate-first.html";
const URL2 = MAIN_DOMAIN + "navigate-second.html";

var EventEmitter = require("devtools/shared/event-emitter");
var client;

SpecialPowers.pushPrefEnv(
  {"set": [["dom.require_user_interaction_for_beforeunload", false]]});

// State machine to check events order
var i = 0;
function assertEvent(event, data) {
  switch (i++) {
    case 0:
      is(event, "request", "Get first page load");
      is(data, URL1);
      break;
    case 1:
      is(event, "load-new-document", "Ask to load the second page");
      break;
    case 2:
      is(event, "unload-dialog", "We get the dialog on first page unload");
      break;
    case 3:
      is(event, "will-navigate", "The very first event is will-navigate on server side");
      is(data.newURI, URL2, "newURI property is correct");
      break;
    case 4:
      is(event, "request",
        "RDP is async with messageManager, the request happens after will-navigate");
      is(data, URL2);
      break;
    case 5:
      is(event, "tabNavigated", "After the request, the client receive tabNavigated");
      is(data.state, "start", "state is start");
      is(data.url, URL2, "url property is correct");
      is(data.nativeConsoleAPI, true, "nativeConsoleAPI is correct");
      break;
    case 6:
      is(event, "DOMContentLoaded");
      // eslint-disable-next-line mozilla/no-cpows-in-tests
      is(content.document.readyState, "interactive");
      break;
    case 7:
      is(event, "load");
      // eslint-disable-next-line mozilla/no-cpows-in-tests
      is(content.document.readyState, "complete");
      break;
    case 8:
      is(event, "navigate",
        "Then once the second doc is loaded, we get the navigate event");
      // eslint-disable-next-line mozilla/no-cpows-in-tests
      is(content.document.readyState, "complete",
        "navigate is emitted only once the document is fully loaded");
      break;
    case 9:
      is(event, "tabNavigated", "Finally, the receive the client event");
      is(data.state, "stop", "state is stop");
      is(data.url, URL2, "url property is correct");
      is(data.nativeConsoleAPI, true, "nativeConsoleAPI is correct");

      // End of test!
      cleanup();
      break;
  }
}

function waitForOnBeforeUnloadDialog(browser, callback) {
  browser.addEventListener("DOMWillOpenModalDialog", function () {
    executeSoon(() => {
      let stack = browser.parentNode;
      let dialogs = stack.getElementsByTagName("tabmodalprompt");
      let {button0, button1} = dialogs[0].ui;
      callback(button0, button1);
    });
  }, {capture: true, once: true});
}

var httpObserver = function (subject, topic, state) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  let url = channel.URI.spec;
  // Only listen for our document request, as many other requests can happen
  if (url == URL1 || url == URL2) {
    assertEvent("request", url);
  }
};
Services.obs.addObserver(httpObserver, "http-on-modify-request");

function onDOMContentLoaded() {
  assertEvent("DOMContentLoaded");
}
function onLoad() {
  assertEvent("load");
}

function getServerTabActor(callback) {
  // Ensure having a minimal server
  initDebuggerServer();

  // Connect to this tab
  let transport = DebuggerServer.connectPipe();
  client = new DebuggerClient(transport);
  connectDebuggerClient(client).then(form => {
    let actorID = form.actor;
    client.attachTab(actorID, function (response, tabClient) {
      // !Hack! Retrieve a server side object, the BrowserTabActor instance
      let tabActor = DebuggerServer.searchAllConnectionsForActor(actorID);
      callback(tabActor);
    });
  });

  client.addListener("tabNavigated", function (event, packet) {
    assertEvent("tabNavigated", packet);
  });
}

function test() {
  // Open a test tab
  addTab(URL1).then(function (browser) {
    getServerTabActor(function (tabActor) {
      // In order to listen to internal will-navigate/navigate events
      EventEmitter.on(tabActor, "will-navigate", function (data) {
        assertEvent("will-navigate", data);
      });
      EventEmitter.on(tabActor, "navigate", function (data) {
        assertEvent("navigate", data);
      });

      // Start listening for page load events
      browser.addEventListener("DOMContentLoaded", onDOMContentLoaded, true);
      browser.addEventListener("load", onLoad, true);

      // Listen for alert() call being made in navigate-first during unload
      waitForOnBeforeUnloadDialog(browser, function (btnLeave, btnStay) {
        assertEvent("unload-dialog");
        // accept to quit this page to another
        btnLeave.click();
      });

      // Load another document in this doc to dispatch these events
      assertEvent("load-new-document");
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, URL2);
    });
  });
}

function cleanup() {
  let browser = gBrowser.selectedBrowser;
  browser.removeEventListener("DOMContentLoaded", onDOMContentLoaded);
  browser.removeEventListener("load", onLoad);
  client.close().then(function () {
    Services.obs.addObserver(httpObserver, "http-on-modify-request");
    DebuggerServer.destroy();
    finish();
  });
}
