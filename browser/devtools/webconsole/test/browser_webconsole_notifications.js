/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "notifications";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let consoleOpened = promise.defer();
  let gotEvents = waitForEvents(consoleOpened.promise);
  yield openConsole().then(() => {
    consoleOpened.resolve();
  });

  yield gotEvents;
});

function waitForEvents(onConsoleOpened) {
  let deferred = promise.defer();

  function webConsoleCreated(id) {
    Services.obs.removeObserver(observer, "web-console-created");
    ok(HUDService.getHudReferenceById(id), "We have a hud reference");
    content.wrappedJSObject.console.log("adding a log message");
  }

  function webConsoleDestroyed(id) {
    Services.obs.removeObserver(observer, "web-console-destroyed");
    ok(!HUDService.getHudReferenceById(id), "We do not have a hud reference");
    executeSoon(deferred.resolve);
  }

  function webConsoleMessage(id, nodeID) {
    Services.obs.removeObserver(observer, "web-console-message-created");
    ok(id, "we have a console ID");
    is(typeof nodeID, "string", "message node id is a string");
    onConsoleOpened.then(closeConsole);
  }

  let observer = {

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

    observe: function observe(subject, topic, data) {
      subject = subject.QueryInterface(Ci.nsISupportsString);

      switch (topic) {
        case "web-console-created":
          webConsoleCreated(subject.data);
          break;
        case "web-console-destroyed":
          webConsoleDestroyed(subject.data);
          break;
        case "web-console-message-created":
          webConsoleMessage(subject, data);
          break;
        default:
          break;
      }
    },

    init: function init() {
      Services.obs.addObserver(this, "web-console-created", false);
      Services.obs.addObserver(this, "web-console-destroyed", false);
      Services.obs.addObserver(this, "web-console-message-created", false);
    }
  };

  observer.init();

  return deferred.promise;
}
