/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
  "resource://gre/modules/WebChannel.jsm");

const HTTP_PATH = "http://example.com";
const HTTP_ENDPOINT = "/browser/browser/base/content/test/general/browser_web_channel.html";

// Keep this synced with /mobile/android/tests/browser/robocop/testWebChannel.js
// as much as possible.  (We only have that since we can't run browser chrome
// tests on Android.  Yet?)
let gTests = [
  {
    desc: "WebChannel generic message",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("generic", Services.io.newURI(HTTP_PATH, null, null));
        channel.listen(function (id, message, target) {
          is(id, "generic");
          is(message.something.nested, "hello");
          channel.stopListening();
          gBrowser.removeTab(tab);
          resolve();
        });

        tab = gBrowser.addTab(HTTP_PATH + HTTP_ENDPOINT + "?generic");
      });
    }
  },
  {
    desc: "WebChannel two way communication",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("twoway", Services.io.newURI(HTTP_PATH, null, null));

        channel.listen(function (id, message, sender) {
          is(id, "twoway");
          ok(message.command);

          if (message.command === "one") {
            channel.send({ data: { nested: true } }, sender);
          }

          if (message.command === "two") {
            is(message.detail.data.nested, true);
            channel.stopListening();
            gBrowser.removeTab(tab);
            resolve();
          }
        });

        tab = gBrowser.addTab(HTTP_PATH + HTTP_ENDPOINT + "?twoway");
      });
    }
  },
  {
    desc: "WebChannel multichannel",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("multichannel", Services.io.newURI(HTTP_PATH, null, null));

        channel.listen(function (id, message, sender) {
          is(id, "multichannel");
          gBrowser.removeTab(tab);
          resolve();
        });

        tab = gBrowser.addTab(HTTP_PATH + HTTP_ENDPOINT + "?multichannel");
      });
    }
  }
]; // gTests

function test() {
  waitForExplicitFinish();

  Task.spawn(function () {
    for (let test of gTests) {
      info("Running: " + test.desc);
      yield test.run();
    }
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
