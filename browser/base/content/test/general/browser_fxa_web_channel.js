/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "WebChannel",
                                  "resource://gre/modules/WebChannel.jsm");

// FxAccountsWebChannel isn't explicitly exported by FxAccountsWebChannel.jsm
// but we can get it here via a backstage pass.
var {FxAccountsWebChannel} = Components.utils.import("resource://gre/modules/FxAccountsWebChannel.jsm", {});

const TEST_HTTP_PATH = "http://example.com";
const TEST_BASE_URL = TEST_HTTP_PATH + "/browser/browser/base/content/test/general/browser_fxa_web_channel.html";
const TEST_CHANNEL_ID = "account_updates_test";

var gTests = [
  {
    desc: "FxA Web Channel - should receive message about profile changes",
    *run() {
      let client = new FxAccountsWebChannel({
        content_uri: TEST_HTTP_PATH,
        channel_id: TEST_CHANNEL_ID,
      });
      let promiseObserver = new Promise((resolve, reject) => {
        makeObserver(FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION, function(subject, topic, data) {
          Assert.equal(data, "abc123");
          client.tearDown();
          resolve();
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: TEST_BASE_URL + "?profile_change"
      }, function* () {
        yield promiseObserver;
      });
    }
  },
  {
    desc: "fxa web channel - login messages should notify the fxAccounts object",
    *run() {

      let promiseLogin = new Promise((resolve, reject) => {
        let login = (accountData) => {
          Assert.equal(typeof accountData.authAt, 'number');
          Assert.equal(accountData.email, 'testuser@testuser.com');
          Assert.equal(accountData.keyFetchToken, 'key_fetch_token');
          Assert.equal(accountData.sessionToken, 'session_token');
          Assert.equal(accountData.uid, 'uid');
          Assert.equal(accountData.unwrapBKey, 'unwrap_b_key');
          Assert.equal(accountData.verified, true);

          client.tearDown();
          resolve();
        };

        let client = new FxAccountsWebChannel({
          content_uri: TEST_HTTP_PATH,
          channel_id: TEST_CHANNEL_ID,
          helpers: {
            login
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: TEST_BASE_URL + "?login"
      }, function* () {
        yield promiseLogin;
      });
    }
  },
  {
    desc: "fxa web channel - can_link_account messages should respond",
    *run() {
      let properUrl = TEST_BASE_URL + "?can_link_account";

      let promiseEcho = new Promise((resolve, reject) => {

        let webChannelOrigin = Services.io.newURI(properUrl);
        // responses sent to content are echoed back over the
        // `fxaccounts_webchannel_response_echo` channel. Ensure the
        // fxaccounts:can_link_account message is responded to.
        let echoWebChannel = new WebChannel('fxaccounts_webchannel_response_echo', webChannelOrigin);
        echoWebChannel.listen((webChannelId, message, target) => {
          Assert.equal(message.command, 'fxaccounts:can_link_account');
          Assert.equal(message.messageId, 2);
          Assert.equal(message.data.ok, true);

          client.tearDown();
          echoWebChannel.stopListening();

          resolve();
        });

        let client = new FxAccountsWebChannel({
          content_uri: TEST_HTTP_PATH,
          channel_id: TEST_CHANNEL_ID,
          helpers: {
            shouldAllowRelink(acctName) {
              return acctName === 'testuser@testuser.com';
            }
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: properUrl
      }, function* () {
        yield promiseEcho;
      });
    }
  },
  {
    desc: "fxa web channel - logout messages should notify the fxAccounts object",
    *run() {
      let promiseLogout = new Promise((resolve, reject) => {
        let logout = (uid) => {
          Assert.equal(uid, 'uid');

          client.tearDown();
          resolve();
        };

        let client = new FxAccountsWebChannel({
          content_uri: TEST_HTTP_PATH,
          channel_id: TEST_CHANNEL_ID,
          helpers: {
            logout
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: TEST_BASE_URL + "?logout"
      }, function* () {
        yield promiseLogout;
      });
    }
  },
  {
    desc: "fxa web channel - delete messages should notify the fxAccounts object",
    *run() {
      let promiseDelete = new Promise((resolve, reject) => {
        let logout = (uid) => {
          Assert.equal(uid, 'uid');

          client.tearDown();
          resolve();
        };

        let client = new FxAccountsWebChannel({
          content_uri: TEST_HTTP_PATH,
          channel_id: TEST_CHANNEL_ID,
          helpers: {
            logout
          }
        });
      });

      yield BrowserTestUtils.withNewTab({
        gBrowser,
        url: TEST_BASE_URL + "?delete"
      }, function* () {
        yield promiseDelete;
      });
    }
  }
]; // gTests

function makeObserver(aObserveTopic, aObserveFunc) {
  let callback = function(aSubject, aTopic, aData) {
    if (aTopic == aObserveTopic) {
      removeMe();
      aObserveFunc(aSubject, aTopic, aData);
    }
  };

  function removeMe() {
    Services.obs.removeObserver(callback, aObserveTopic);
  }

  Services.obs.addObserver(callback, aObserveTopic, false);
  return removeMe;
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function* () {
    for (let testCase of gTests) {
      info("Running: " + testCase.desc);
      yield testCase.run();
    }
  }).then(finish, ex => {
    Assert.ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
