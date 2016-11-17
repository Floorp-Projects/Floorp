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
const HTTP_MISMATCH_PATH = "http://example.org";
const HTTP_IFRAME_PATH = "http://mochi.test:8888";
const HTTP_REDIRECTED_IFRAME_PATH = "http://example.org";

// Keep this synced with /mobile/android/tests/browser/robocop/testWebChannel.js
// as much as possible.  (We only have that since we can't run browser chrome
// tests on Android.  Yet?)
var gTests = [
  {
    desc: "WebChannel generic message",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("generic", Services.io.newURI(HTTP_PATH, null, null));
        channel.listen(function(id, message, target) {
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

        channel.listen(function(id, message, sender) {
          is(id, "twoway", "bad id");
          ok(message.command, "command not ok");

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
    desc: "WebChannel two way communication in an iframe",
    run: function* () {
      let parentChannel = new WebChannel("echo", Services.io.newURI(HTTP_PATH, null, null));
      let iframeChannel = new WebChannel("twoway", Services.io.newURI(HTTP_IFRAME_PATH, null, null));
      let promiseTestDone = new Promise(function(resolve, reject) {
        parentChannel.listen(function(id, message, sender) {
          reject(new Error("WebChannel message incorrectly sent to parent"));
        });

        iframeChannel.listen(function(id, message, sender) {
          is(id, "twoway", "bad id (2)");
          ok(message.command, "command not ok (2)");

          if (message.command === "one") {
            iframeChannel.send({ data: { nested: true } }, sender);
          }

          if (message.command === "two") {
            is(message.detail.data.nested, true);
            resolve();
          }
        });
      });
      yield BrowserTestUtils.withNewTab({
        gBrowser: gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?iframe"
      }, function* () {
        yield promiseTestDone;
        parentChannel.stopListening();
        iframeChannel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel response to a redirected iframe",
    run: function* () {
      /**
       * This test checks that WebChannel responses are only sent
       * to an iframe if the iframe has not redirected to another origin.
       * Test flow:
       * 1. create a page, embed an iframe on origin A.
       * 2. the iframe sends a message `redirecting`, then redirects to
       *    origin B.
       * 3. the iframe at origin B is set up to echo any messages back to the
       *    test parent.
       * 4. the test parent receives the `redirecting` message from origin A.
       *    the test parent creates a new channel with origin B.
       * 5. when origin B is ready, it sends a `loaded` message to the test
       *    parent, letting the test parent know origin B is ready to echo
       *    messages.
       * 5. the test parent tries to send a response to origin A. If the
       *    WebChannel does not perform a valid origin check, the response
       *    will be received by origin B. If the WebChannel does perform
       *    a valid origin check, the response will not be sent.
       * 6. the test parent sends a `done` message to origin B, which origin
       *    B echoes back. If the response to origin A is not echoed but
       *    the message to origin B is, then hooray, the test passes.
       */

      let preRedirectChannel = new WebChannel("pre_redirect", Services.io.newURI(HTTP_IFRAME_PATH, null, null));
      let postRedirectChannel = new WebChannel("post_redirect", Services.io.newURI(HTTP_REDIRECTED_IFRAME_PATH, null, null));

      let promiseTestDone = new Promise(function(resolve, reject) {
        preRedirectChannel.listen(function(id, message, preRedirectSender) {
          if (message.command === "redirecting") {

            postRedirectChannel.listen(function(aId, aMessage, aPostRedirectSender) {
              is(aId, "post_redirect");
              isnot(aMessage.command, "no_response_expected");

              if (aMessage.command === "loaded") {
                // The message should not be received on the preRedirectChannel
                // because the target window has redirected.
                preRedirectChannel.send({ command: "no_response_expected" }, preRedirectSender);
                postRedirectChannel.send({ command: "done" }, aPostRedirectSender);
              } else if (aMessage.command === "done") {
                resolve();
              } else {
                reject(new Error(`Unexpected command ${aMessage.command}`));
              }
            });
          } else {
            reject(new Error(`Unexpected command ${message.command}`));
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser: gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?iframe_pre_redirect"
      }, function* () {
        yield promiseTestDone;
        preRedirectChannel.stopListening();
        postRedirectChannel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel multichannel",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tab;
        let channel = new WebChannel("multichannel", Services.io.newURI(HTTP_PATH, null, null));

        channel.listen(function(id, message, sender) {
          is(id, "multichannel");
          gBrowser.removeTab(tab);
          resolve();
        });

        tab = gBrowser.addTab(HTTP_PATH + HTTP_ENDPOINT + "?multichannel");
      });
    }
  },
  {
    desc: "WebChannel unsolicited send, using system principal",
    run: function* () {
      let channel = new WebChannel("echo", Services.io.newURI(HTTP_PATH, null, null));

      // an unsolicted message is sent from Chrome->Content which is then
      // echoed back. If the echo is received here, then the content
      // received the message.
      let messagePromise = new Promise(function(resolve, reject) {
        channel.listen(function(id, message, sender) {
          is(id, "echo");
          is(message.command, "unsolicited");

          resolve()
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?unsolicited"
      }, function* (targetBrowser) {
        channel.send({ command: "unsolicited" }, {
          browser: targetBrowser,
          principal: Services.scriptSecurityManager.getSystemPrincipal()
        });
        yield messagePromise;
        channel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel unsolicited send, using target origin's principal",
    run: function* () {
      let targetURI = Services.io.newURI(HTTP_PATH, null, null);
      let channel = new WebChannel("echo", targetURI);

      // an unsolicted message is sent from Chrome->Content which is then
      // echoed back. If the echo is received here, then the content
      // received the message.
      let messagePromise = new Promise(function(resolve, reject) {
        channel.listen(function(id, message, sender) {
          is(id, "echo");
          is(message.command, "unsolicited");

          resolve();
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?unsolicited"
      }, function* (targetBrowser) {

        channel.send({ command: "unsolicited" }, {
          browser: targetBrowser,
          principal: Services.scriptSecurityManager.getNoAppCodebasePrincipal(targetURI)
        });

        yield messagePromise;
        channel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel unsolicited send with principal mismatch",
    run: function* () {
      let targetURI = Services.io.newURI(HTTP_PATH, null, null);
      let channel = new WebChannel("echo", targetURI);

      // two unsolicited messages are sent from Chrome->Content. The first,
      // `unsolicited_no_response_expected` is sent to the wrong principal
      // and should not be echoed back. The second, `done`, is sent to the
      // correct principal and should be echoed back.
      let messagePromise = new Promise(function(resolve, reject) {
        channel.listen(function(id, message, sender) {
          is(id, "echo");

          if (message.command === "done") {
            resolve();
          } else {
            reject(new Error(`Unexpected command ${message.command}`));
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser: gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?unsolicited"
      }, function* (targetBrowser) {

        let mismatchURI = Services.io.newURI(HTTP_MISMATCH_PATH, null, null);
        let mismatchPrincipal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(mismatchURI);

        // send a message to the wrong principal. It should not be delivered
        // to content, and should not be echoed back.
        channel.send({ command: "unsolicited_no_response_expected" }, {
          browser: targetBrowser,
          principal: mismatchPrincipal
        });

        let targetPrincipal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(targetURI);

        // send the `done` message to the correct principal. It
        // should be echoed back.
        channel.send({ command: "done" }, {
          browser: targetBrowser,
          principal: targetPrincipal
        });

        yield messagePromise;
        channel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel non-window target",
    run: function* () {
      /**
       * This test ensures messages can be received from and responses
       * sent to non-window elements.
       *
       * First wait for the non-window element to send a "start" message.
       * Then send the non-window element a "done" message.
       * The non-window element will echo the "done" message back, if it
       * receives the message.
       * Listen for the response. If received, good to go!
       */
      let channel = new WebChannel("not_a_window", Services.io.newURI(HTTP_PATH, null, null));

      let testDonePromise = new Promise(function(resolve, reject) {
        channel.listen(function(id, message, sender) {
          if (message.command === "start") {
            channel.send({ command: "done" }, sender);
          } else if (message.command === "done") {
            resolve();
          } else {
            reject(new Error(`Unexpected command ${message.command}`));
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?bubbles"
      }, function* () {
        yield testDonePromise;
        channel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel disallows non-string message from non-whitelisted origin",
    run: function* () {
      /**
       * This test ensures that non-string messages can't be sent via WebChannels.
       * We create a page (on a non-whitelisted origin) which should send us two
       * messages immediately. The first message has an object for it's detail,
       * and the second has a string. We check that we only get the second
       * message.
       */
      let channel = new WebChannel("objects", Services.io.newURI(HTTP_PATH, null, null));
      let testDonePromise = new Promise((resolve, reject) => {
        channel.listen((id, message, sender) => {
          is(id, "objects");
          is(message.type, "string");
          resolve();
        });
      });
      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?object"
      }, function* () {
        yield testDonePromise;
        channel.stopListening();
      });
    }
  },
  {
    desc: "WebChannel allows both string and non-string message from whitelisted origin",
    run: function* () {
      /**
       * Same process as above, but we whitelist the origin before loading the page,
       * and expect to get *both* messages back (each exactly once).
       */
      let channel = new WebChannel("objects", Services.io.newURI(HTTP_PATH, null, null));

      let testDonePromise = new Promise((resolve, reject) => {
        let sawObject = false;
        let sawString = false;
        channel.listen((id, message, sender) => {
          is(id, "objects");
          if (message.type === "object") {
            ok(!sawObject);
            sawObject = true;
          } else if (message.type === "string") {
            ok(!sawString);
            sawString = true;
          } else {
            reject(new Error(`Unknown message type: ${message.type}`))
          }
          if (sawObject && sawString) {
            resolve();
          }
        });
      });
      const webchannelWhitelistPref = "webchannel.allowObject.urlWhitelist";
      let origWhitelist = Services.prefs.getCharPref(webchannelWhitelistPref);
      let newWhitelist = origWhitelist + " " + HTTP_PATH;
      Services.prefs.setCharPref(webchannelWhitelistPref, newWhitelist);
      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: HTTP_PATH + HTTP_ENDPOINT + "?object"
      }, function* () {
        yield testDonePromise;
        Services.prefs.setCharPref(webchannelWhitelistPref, origWhitelist);
        channel.stopListening();
      });
    }
  }
]; // gTests

function test() {
  waitForExplicitFinish();

  Task.spawn(function* () {
    for (let testCase of gTests) {
      info("Running: " + testCase.desc);
      yield testCase.run();
    }
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
