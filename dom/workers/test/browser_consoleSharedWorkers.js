/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test() {
  const testURL = getRootDirectory(gTestPath) + "empty.html";
  let tab = BrowserTestUtils.addTab(gBrowser, testURL);
  gBrowser.selectedTab = tab;

  await BrowserTestUtils.browserLoaded(gBrowser.getBrowserForTab(tab));

  let promise = new Promise(resolve => {
    const ConsoleAPIStorage = SpecialPowers.Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(SpecialPowers.Ci.nsIConsoleAPIStorage);

    function consoleListener() {
      this.onConsoleLogEvent = this.onConsoleLogEvent.bind(this);
      ConsoleAPIStorage.addLogEventListener(
        this.onConsoleLogEvent,
        SpecialPowers.wrap(document).nodePrincipal
      );
      Services.obs.addObserver(this, "console-api-profiler");
    }

    var order = 0;
    consoleListener.prototype = {
      onConsoleLogEvent(aSubject) {
        var obj = aSubject.wrappedJSObject;
        is(
          obj.arguments[0],
          "Hello world from a SharedWorker!",
          "A message from a SharedWorker \\o/"
        );
        is(obj.ID, "sharedWorker_console.js", "The ID is SharedWorker");
        is(obj.innerID, "SharedWorker", "The ID is SharedWorker");
        is(order++, 1, "Then a log message.");

        ConsoleAPIStorage.removeLogEventListener(this.onConsoleLogEvent);
        resolve();
      },

      observe: (aSubject, aTopic) => {
        ok(true, "Something has been received");

        if (aTopic == "console-api-profiler") {
          var obj = aSubject.wrappedJSObject;
          is(
            obj.arguments[0],
            "Hello profiling from a SharedWorker!",
            "A message from a SharedWorker \\o/"
          );
          is(order++, 0, "First a profiler message.");

          Services.obs.removeObserver(cl, "console-api-profiler");
        }
      },
    };

    var cl = new consoleListener();
  });

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    new content.SharedWorker("sharedWorker_console.js");
  });

  await promise;

  await BrowserTestUtils.removeTab(tab);
});
