/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-cpows-in-tests */
/* global sendAsyncMessage */

"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const HTTP_ROOT = CHROME_ROOT.replace("chrome://mochitests/content/",
                                      "http://mochi.test:8888/");
const SERVICE_WORKER = HTTP_ROOT + "service-workers/empty-sw.js";
const TAB_URL = HTTP_ROOT + "service-workers/empty-sw.html";

const SW_TIMEOUT = 1000;

function assertHasWorker(expected, document, type, name) {
  let names = [...document.querySelectorAll("#" + type + " .target-name")];
  names = names.map(element => element.textContent);
  is(names.includes(name), expected,
      "The " + type + " url appears in the list: " + names);
}

add_task(function* () {
  yield new Promise(done => {
    let options = {"set": [
      // Accept workers from mochitest's http
      ["dom.serviceWorkers.testing.enabled", true],
      // Reduce the timeout to expose issues when service worker
      // freezing is broken
      ["dom.serviceWorkers.idle_timeout", SW_TIMEOUT],
      ["dom.serviceWorkers.idle_extended_timeout", SW_TIMEOUT],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let { tab, document } = yield openAboutDebugging("workers");

  let swTab = yield addTab(TAB_URL);

  let serviceWorkersElement = document.getElementById("service-workers");
  yield waitForMutation(serviceWorkersElement, { childList: true });

  assertHasWorker(true, document, "service-workers", SERVICE_WORKER);

  // Ensure that the registration resolved before trying to connect to the sw
  let frameScript = function() {
    // Retrieve the `sw` promise created in the html page
    let { sw } = content.wrappedJSObject;
    sw.then(function() {
      sendAsyncMessage("sw-registered");
    });
  };
  let mm = swTab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);

  yield new Promise(done => {
    mm.addMessageListener("sw-registered", function listener() {
      mm.removeMessageListener("sw-registered", listener);
      done();
    });
  });
  ok(true, "Service worker registration resolved");

  // Retrieve the DEBUG button for the worker
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  let name = names.filter(element => element.textContent === SERVICE_WORKER)[0];
  ok(name, "Found the service worker in the list");
  let debugBtn = name.parentNode.parentNode.querySelector("button");
  ok(debugBtn, "Found its debug button");

  // Click on it and wait for the toolbox to be ready
  let onToolboxReady = new Promise(done => {
    gDevTools.once("toolbox-ready", function(e, toolbox) {
      done(toolbox);
    });
  });
  debugBtn.click();

  let toolbox = yield onToolboxReady;

  // Wait for more than the regular timeout,
  // so that if the worker freezing doesn't work,
  // it will be destroyed and removed from the list
  yield new Promise(done => {
    setTimeout(done, SW_TIMEOUT * 2);
  });

  assertHasWorker(true, document, "service-workers", SERVICE_WORKER);

  yield toolbox.destroy();
  toolbox = null;

  // Now ensure that the worker is correctly destroyed
  // after we destroy the toolbox.
  // The list should update once it get destroyed.
  yield waitForMutation(serviceWorkersElement, { childList: true });

  assertHasWorker(false, document, "service-workers", SERVICE_WORKER);

  // Finally, unregister the service worker itself
  // Use message manager to work with e10s
  frameScript = function() {
    // Retrieve the `sw` promise created in the html page
    let { sw } = content.wrappedJSObject;
    sw.then(function(registration) {
      registration.unregister().then(function() {
        sendAsyncMessage("sw-unregistered");
      },
      function(e) {
        dump("SW not unregistered; " + e + "\n");
      });
    });
  };
  mm = swTab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);

  yield new Promise(done => {
    mm.addMessageListener("sw-unregistered", function listener() {
      mm.removeMessageListener("sw-unregistered", listener);
      done();
    });
  });
  ok(true, "Service worker registration unregistered");

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
