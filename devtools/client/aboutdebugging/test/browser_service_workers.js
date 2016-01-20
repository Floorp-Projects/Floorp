/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Service workers can't be loaded from chrome://,
// but http:// is ok with dom.serviceWorkers.testing.enabled turned on.
const HTTP_ROOT = CHROME_ROOT.replace("chrome://mochitests/content/",
                                      "http://mochi.test:8888/");
const SERVICE_WORKER = HTTP_ROOT + "service-workers/empty-sw.js";
const TAB_URL = HTTP_ROOT + "service-workers/empty-sw.html";

function waitForWorkersUpdate(document) {
  return new Promise(done => {
    var observer = new MutationObserver(function(mutations) {
      observer.disconnect();
      done();
    });
    var target = document.getElementById("service-workers");
    observer.observe(target, { childList: true });
  });
}

add_task(function *() {
  yield new Promise(done => {
    let options = {"set": [
                    ["dom.serviceWorkers.testing.enabled", true],
                  ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let { tab, document } = yield openAboutDebugging("workers");

  let swTab = yield addTab(TAB_URL);

  yield waitForWorkersUpdate(document);

  // Check that the service worker appears in the UI
  let names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(names.includes(SERVICE_WORKER), "The service worker url appears in the list: " + names);

  // Finally, unregister the service worker itself
  let aboutDebuggingUpdate = waitForWorkersUpdate(document);

  // Use message manager to work with e10s
  let frameScript = function () {
    // Retrieve the `sw` promise created in the html page
    let { sw } = content.wrappedJSObject;
    sw.then(function (registration) {
      registration.unregister().then(function (success) {
        sendAsyncMessage("sw-unregistered");
      },
      function (e) {
        dump("SW not unregistered; " + e + "\n");
      });
    });
  };
  let mm = swTab.linkedBrowser.messageManager;
  mm.loadFrameScript("data:,(" + encodeURIComponent(frameScript) + ")()", true);

  yield new Promise(done => {
    mm.addMessageListener("sw-unregistered", function listener() {
      mm.removeMessageListener("sw-unregistered", listener);
      done();
    });
  });
  ok(true, "Service worker registration unregistered");

  yield aboutDebuggingUpdate;

  // Check that the service worker disappeared from the UI
  names = [...document.querySelectorAll("#service-workers .target-name")];
  names = names.map(element => element.textContent);
  ok(!names.includes(SERVICE_WORKER), "The service worker url is no longer in the list: " + names);

  yield removeTab(swTab);
  yield closeAboutDebugging(tab);
});
