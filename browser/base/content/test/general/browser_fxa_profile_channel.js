/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfileChannel",
  "resource://gre/modules/FxAccountsProfileChannel.jsm");

const HTTP_PATH = "http://example.com";

let gTests = [
  {
    desc: "FxA Profile Channel - should receive message about account updates",
    run: function* () {
      return new Promise(function(resolve, reject) {
        let tabOpened = false;
        let properUrl = "http://example.com/browser/browser/base/content/test/general/browser_fxa_profile_channel.html";

        waitForTab(function (tab) {
          Assert.ok("Tab successfully opened");
          let match = gBrowser.currentURI.spec == properUrl;
          Assert.ok(match);

          tabOpened = true;
        });

        let client = new FxAccountsProfileChannel({
          content_uri: HTTP_PATH,
        });

        makeObserver(FxAccountsCommon.ON_PROFILE_CHANGE_NOTIFICATION, function (subject, topic, data) {
          Assert.ok(tabOpened);
          Assert.equal(data, "abc123");
          resolve();
          gBrowser.removeCurrentTab();
        });

        gBrowser.selectedTab = gBrowser.addTab(properUrl);
      });
    }
  }
]; // gTests

function makeObserver(aObserveTopic, aObserveFunc) {
  let callback = function (aSubject, aTopic, aData) {
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
