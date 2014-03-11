/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

const CHROME_BASE = "chrome://mochitests/content/browser/browser/base/content/test/general/";
// Preference helpers.
let changedPrefs = new Set();

function setPref(name, value) {
  changedPrefs.add(name);
  Services.prefs.setCharPref(name, value);
}

registerCleanupFunction(function() {
  // Ensure we don't pollute prefs for next tests.
  for (let name of changedPrefs) {
    Services.prefs.clearUserPref(name);
  }
});

let gTests = [
{
  desc: "Test the remote commands",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    setPref("identity.fxaccounts.remote.uri",
            "https://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");
    yield promiseNewTabLoadEvent("about:accounts");

    let deferred = Promise.defer();

    let results = 0;
    try {
      let win = gBrowser.contentWindow;
      win.addEventListener("message", function testLoad(e) {
        if (e.data.type == "testResult") {
          ok(e.data.pass, e.data.info);
          results++;
        }
        else if (e.data.type == "testsComplete") {
          is(results, e.data.count, "Checking number of results received matches the number of tests that should have run");
          win.removeEventListener("message", testLoad, false, true);
          deferred.resolve();
        }

      }, false, true);

    } catch(e) {
      ok(false, "Failed to get all commands");
      deferred.reject();
    }
    return deferred.promise.then(() => fxAccounts.signOut());
  }
},
{
  desc: "Test action=signin",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    let [tab, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    is(url, expected_url, "action=signin got the expected URL");
  }
},
{
  desc: "Test action=signup",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    const expected_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.uri", expected_url);
    let [tab, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signup");
    is(url, expected_url, "action=signup got the expected URL");
  },
},
{
  desc: "Test action=reauth",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield fxAccounts.signOut();
  },
  run: function* ()
  {
    const expected_url = "https://example.com/?is_force_auth";
    setPref("identity.fxaccounts.remote.force_auth.uri", expected_url);
    let userData = {
      email: "foo@example.com",
      uid: "1234@lcip.org",
      assertion: "foobar",
      sessionToken: "dead",
      kA: "beef",
      kB: "cafe",
      verified: true
    };

    yield fxAccounts.setSignedInUser(userData);
    let [tab, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=reauth");
    // The current user will be appended to the url
    let expected = expected_url + "&email=foo%40example.com";
    is(url, expected, "action=reauth got the expected URL");
  },
},

]; // gTests

function test()
{
  waitForExplicitFinish();

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);
      try {
        yield test.run();
      } finally {
        yield test.teardown();
      }
    }

    finish();
  });
}

function promiseNewTabLoadEvent(aUrl)
{
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  // give it an e10s-friendly content script to help with our tests.
  mm.loadFrameScript(CHROME_BASE + "content_aboutAccounts.js", true);
  // and wait for it to tell us about the load.
  mm.addMessageListener("test:document:load", function onLoad() {
    mm.removeMessageListener("test:document:load", onLoad);
    deferred.resolve(tab);
  });
  return deferred.promise;
}

// Returns a promise which is resolved with the iframe's URL after a new
// tab is created and the iframe in that tab loads.
function promiseNewTabWithIframeLoadEvent(aUrl) {
  let deferred = Promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  // give it an e10s-friendly content script to help with our tests.
  mm.loadFrameScript(CHROME_BASE + "content_aboutAccounts.js", true);
  // and wait for it to tell us about the iframe load.
  mm.addMessageListener("test:iframe:load", function onFrameLoad(message) {
    mm.removeMessageListener("test:iframe:load", onFrameLoad);
    deferred.resolve([tab, message.data.url]);
  });
  return deferred.promise;
}
