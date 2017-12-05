/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "notifications";

add_task(function* () {
  yield loadTab(TEST_URI);

  let consoleOpened = defer();
  let gotEvents = waitForEvents(consoleOpened.promise);
  yield openConsole().then(() => {
    consoleOpened.resolve();
  });

  yield gotEvents;
});

function waitForEvents(onConsoleOpened) {
  let deferred = defer();

  function webConsoleCreated(id) {
    Services.obs.removeObserver(observer, "web-console-created");
    ok(HUDService.getHudReferenceById(id), "We have a hud reference");
    gBrowser.contentWindowAsCPOW.wrappedJSObject.console.log("adding a log message");
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
      Services.obs.addObserver(this, "web-console-created");
      Services.obs.addObserver(this, "web-console-destroyed");
      Services.obs.addObserver(this, "web-console-message-created");
    }
  };

  observer.init();

  return deferred.promise;
}
