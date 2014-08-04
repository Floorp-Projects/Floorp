/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsOAuthClient",
  "resource://gre/modules/FxAccountsOAuthClient.jsm");

const HTTP_PATH = "http://example.com";
const HTTP_ENDPOINT = "/browser/browser/base/content/test/general/browser_fxa_oauth.html";

let gTests = [
  {
    desc: "FxA OAuth - should open a new tab, complete OAuth flow",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;
        let properUrl = "http://example.com/browser/browser/base/content/test/general/browser_fxa_oauth.html?" +
          "webChannelId=oauth_client_id&scope=&client_id=client_id&action=signin&state=state";
        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");
          Assert.equal(gBrowser.currentURI.spec, properUrl);
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

  Task.spawn(function () {
    for (let test of gTests) {
      info("Running: " + test.desc);
      yield test.run();
    }
  }).then(finish, ex => {
    Assert.ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
