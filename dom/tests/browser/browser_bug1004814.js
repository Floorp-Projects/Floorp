/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test_bug1004814.html";

add_task(function*() {
  yield BrowserTestUtils.withNewTab(TEST_URI, function*(aBrowser) {
    let duration = yield ContentTask.spawn(aBrowser, null, function (opts) {
      return new Promise(resolve => {
        let ConsoleObserver = {
          QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

          observe: function(aSubject, aTopic, aData) {
            var obj = aSubject.wrappedJSObject;
            if (obj.arguments.length != 1 || obj.arguments[0] != 'bug1004814' ||
                obj.level != 'timeEnd') {
              return;
            }

            Services.obs.removeObserver(this, "console-api-log-event");
            resolve(obj.timer.duration);
          }
        };

        Services.obs.addObserver(ConsoleObserver, "console-api-log-event", false);
      });
    });

    ok(duration > 0, "ConsoleEvent.timer.duration > 0: " + duration + " ~ 200ms");
  });
});
