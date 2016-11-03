/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: this.docShell is null");

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsOAuthClient",
  "resource://gre/modules/FxAccountsOAuthClient.jsm");

const HTTP_PATH = "http://example.com";
const HTTP_ENDPOINT = "/browser/browser/base/content/test/general/browser_fxa_oauth.html";
const HTTP_ENDPOINT_WITH_KEYS = "/browser/browser/base/content/test/general/browser_fxa_oauth_with_keys.html";

var gTests = [
  {
    desc: "FxA OAuth - should open a new tab, complete OAuth flow",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;
        let properURL = "http://example.com/browser/browser/base/content/test/general/browser_fxa_oauth.html";
        let queryStrings = [
          "action=signin",
          "client_id=client_id",
          "scope=",
          "state=state",
          "webChannelId=oauth_client_id",
        ];
        queryStrings.sort();

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");
          Assert.ok(gBrowser.currentURI.spec.split("?")[0], properURL, "Check URL without params");
          let actualURL = new URL(gBrowser.currentURI.spec);
          let actualQueryStrings = actualURL.search.substring(1).split("&");
          actualQueryStrings.sort();
          Assert.equal(actualQueryStrings.length, queryStrings.length, "Check number of params");

          for (let i = 0; i < queryStrings.length; i++) {
            Assert.equal(actualQueryStrings[i], queryStrings[i], "Check parameter " + i);
          }

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH,
          },
          authorizationEndpoint: HTTP_ENDPOINT
        });

        client.onComplete = function(tokenData) {
          Assert.ok(tabOpened);
          Assert.equal(tokenData.code, "code1");
          Assert.equal(tokenData.state, "state");
          resolve();
        };

        client.onError = reject;

        client.launchWebFlow();
      });
    }
  },
  {
    desc: "FxA OAuth - should open a new tab, complete OAuth flow when forcing auth",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;
        let properURL = "http://example.com/browser/browser/base/content/test/general/browser_fxa_oauth.html";
        let queryStrings = [
          "action=force_auth",
          "client_id=client_id",
          "scope=",
          "state=state",
          "webChannelId=oauth_client_id",
          "email=test%40invalid.com",
        ];
        queryStrings.sort();

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");
          Assert.ok(gBrowser.currentURI.spec.split("?")[0], properURL, "Check URL without params");

          let actualURL = new URL(gBrowser.currentURI.spec);
          let actualQueryStrings = actualURL.search.substring(1).split("&");
          actualQueryStrings.sort();
          Assert.equal(actualQueryStrings.length, queryStrings.length, "Check number of params");

          for (let i = 0; i < queryStrings.length; i++) {
            Assert.equal(actualQueryStrings[i], queryStrings[i], "Check parameter " + i);
          }

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH,
            action: "force_auth",
            email: "test@invalid.com"
          },
          authorizationEndpoint: HTTP_ENDPOINT
        });

        client.onComplete = function(tokenData) {
          Assert.ok(tabOpened);
          Assert.equal(tokenData.code, "code1");
          Assert.equal(tokenData.state, "state");
          resolve();
        };

        client.onError = reject;

        client.launchWebFlow();
      });
    }
  },
  {
    desc: "FxA OAuth - should receive an error when there's a state mismatch",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");

          // It should have passed in the expected non-matching state value.
          let queryString = gBrowser.currentURI.spec.split("?")[1];
          Assert.ok(queryString.indexOf('state=different-state') >= 0);

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "different-state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH,
          },
          authorizationEndpoint: HTTP_ENDPOINT
        });

        client.onComplete = reject;

        client.onError = function(err) {
          Assert.ok(tabOpened);
          Assert.equal(err.message, "OAuth flow failed. State doesn't match");
          resolve();
        };

        client.launchWebFlow();
      });
    }
  },
  {
    desc: "FxA OAuth - should be able to request keys during OAuth flow",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");

          // It should have asked for keys.
          let queryString = gBrowser.currentURI.spec.split('?')[1];
          Assert.ok(queryString.indexOf('keys=true') >= 0);

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH,
            keys: true,
          },
          authorizationEndpoint: HTTP_ENDPOINT_WITH_KEYS
        });

        client.onComplete = function(tokenData, keys) {
          Assert.ok(tabOpened);
          Assert.equal(tokenData.code, "code1");
          Assert.equal(tokenData.state, "state");
          Assert.deepEqual(keys.kAr, {k: "kAr"});
          Assert.deepEqual(keys.kBr, {k: "kBr"});
          resolve();
        };

        client.onError = reject;

        client.launchWebFlow();
      });
    }
  },
  {
    desc: "FxA OAuth - should not receive keys if not explicitly requested",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");

          // It should not have asked for keys.
          let queryString = gBrowser.currentURI.spec.split('?')[1];
          Assert.ok(queryString.indexOf('keys=true') == -1);

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH
          },
          // This endpoint will cause the completion message to contain keys.
          authorizationEndpoint: HTTP_ENDPOINT_WITH_KEYS
        });

        client.onComplete = function(tokenData, keys) {
          Assert.ok(tabOpened);
          Assert.equal(tokenData.code, "code1");
          Assert.equal(tokenData.state, "state");
          Assert.strictEqual(keys, undefined);
          resolve();
        };

        client.onError = reject;

        client.launchWebFlow();
      });
    }
  },
  {
    desc: "FxA OAuth - should receive an error if keys could not be obtained",
    run: function () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");

          // It should have asked for keys.
          let queryString = gBrowser.currentURI.spec.split('?')[1];
          Assert.ok(queryString.indexOf('keys=true') >= 0);

          tabOpened = true;
        });

        let client = new FxAccountsOAuthClient({
          parameters: {
            state: "state",
            client_id: "client_id",
            oauth_uri: HTTP_PATH,
            content_uri: HTTP_PATH,
            keys: true,
          },
          // This endpoint will cause the completion message not to contain keys.
          authorizationEndpoint: HTTP_ENDPOINT
        });

        client.onComplete = reject;

        client.onError = function(err) {
          Assert.ok(tabOpened);
          Assert.equal(err.message, "OAuth flow failed. Keys were not returned");
          resolve();
        };

        client.launchWebFlow();
      });
    }
  }
]; // gTests

function waitForTab(aCallback) {
  let container = gBrowser.tabContainer;
  container.addEventListener("TabOpen", function tabOpener(event) {
    container.removeEventListener("TabOpen", tabOpener, false);
    gBrowser.addEventListener("load", function listener() {
      gBrowser.removeEventListener("load", listener, true);
      let tab = event.target;
      aCallback(tab);
    }, true);
  }, false);
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function* () {
    const webchannelWhitelistPref = "webchannel.allowObject.urlWhitelist";
    let origWhitelist = Services.prefs.getCharPref(webchannelWhitelistPref);
    let newWhitelist = origWhitelist + " http://example.com";
    Services.prefs.setCharPref(webchannelWhitelistPref, newWhitelist);
    try {
      for (let testCase of gTests) {
        info("Running: " + testCase.desc);
        yield testCase.run();
      }
    } finally {
      Services.prefs.clearUserPref(webchannelWhitelistPref);
    }
  }).then(finish, ex => {
    Assert.ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
