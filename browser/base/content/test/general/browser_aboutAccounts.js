/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: window.location is null");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

const CHROME_BASE = "chrome://mochitests/content/browser/browser/base/content/test/general/";
// Preference helpers.
var changedPrefs = new Set();

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

var gTests = [
{
  desc: "Test the remote commands",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield signOut();
  },
  run: function* ()
  {
    setPref("identity.fxaccounts.remote.signup.uri",
            "https://example.com/browser/browser/base/content/test/general/accounts_testRemoteCommands.html");
    let tab = yield promiseNewTabLoadEvent("about:accounts");
    let mm = tab.linkedBrowser.messageManager;

    let deferred = Promise.defer();

    // We'll get a message when openPrefs() is called, which this test should
    // arrange.
    let promisePrefsOpened = promiseOneMessage(tab, "test:openPrefsCalled");
    let results = 0;
    try {
      mm.addMessageListener("test:response", function responseHandler(msg) {
        let data = msg.data.data;
        if (data.type == "testResult") {
          ok(data.pass, data.info);
          results++;
        } else if (data.type == "testsComplete") {
          is(results, data.count, "Checking number of results received matches the number of tests that should have run");
          mm.removeMessageListener("test:response", responseHandler);
          deferred.resolve();
        }
      });
    } catch (e) {
      ok(false, "Failed to get all commands");
      deferred.reject();
    }
    yield deferred.promise;
    yield promisePrefsOpened;
  }
},
{
  desc: "Test action=signin - no user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    // When this loads with no user logged-in, we expect the "normal" URL
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    let [tab, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    is(url, expected_url, "action=signin got the expected URL");
    // we expect the remote iframe to be shown.
    yield checkVisibilities(tab, {
      stage: false, // parent of 'manage' and 'intro'
      manage: false,
      intro: false, // this is  "get started"
      remote: true,
      networkError: false
    });
  }
},
{
  desc: "Test action=signin - user logged in",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield signOut();
  },
  run: function* ()
  {
    // When this loads with a user logged-in, we expect the normal URL to
    // have been ignored and the "manage" page to be shown.
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    yield setSignedInUser();
    let tab = yield promiseNewTabLoadEvent("about:accounts?action=signin");
    // about:accounts initializes after fetching the current user from Fxa -
    // so we also request it - by the time we get it we know it should have
    // done its thing.
    yield fxAccounts.getSignedInUser();
    // we expect "manage" to be shown.
    yield checkVisibilities(tab, {
      stage: true, // parent of 'manage' and 'intro'
      manage: true,
      intro: false, // this is  "get started"
      remote: false,
      networkError: false
    });
  }
},
{
  desc: "Test action=signin - captive portal",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    const signinUrl = "https://redirproxy.example.com/test";
    setPref("identity.fxaccounts.remote.signin.uri", signinUrl);
    let [tab, ] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    yield checkVisibilities(tab, {
      stage: true, // parent of 'manage' and 'intro'
      manage: false,
      intro: false, // this is  "get started"
      remote: false,
      networkError: true
    });
  }
},
{
  desc: "Test action=signin - offline",
  teardown: () => {
    gBrowser.removeCurrentTab();
    BrowserOffline.toggleOfflineStatus();
  },
  run: function* ()
  {
    BrowserOffline.toggleOfflineStatus();
    Services.cache2.clear();

    const signinUrl = "https://unknowndomain.cow";
    setPref("identity.fxaccounts.remote.signin.uri", signinUrl);
    let [tab, ] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    yield checkVisibilities(tab, {
      stage: true, // parent of 'manage' and 'intro'
      manage: false,
      intro: false, // this is  "get started"
      remote: false,
      networkError: true
    });
  }
},
{
  desc: "Test action=signup - no user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    const expected_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", expected_url);
    let [tab, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signup");
    is(url, expected_url, "action=signup got the expected URL");
    // we expect the remote iframe to be shown.
    yield checkVisibilities(tab, {
      stage: false, // parent of 'manage' and 'intro'
      manage: false,
      intro: false, // this is  "get started"
      remote: true,
      networkError: false
    });
  },
},
{
  desc: "Test action=signup - user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* ()
  {
    const expected_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", expected_url);
    yield setSignedInUser();
    let tab = yield promiseNewTabLoadEvent("about:accounts?action=signup");
    yield fxAccounts.getSignedInUser();
    // we expect "manage" to be shown.
    yield checkVisibilities(tab, {
      stage: true, // parent of 'manage' and 'intro'
      manage: true,
      intro: false, // this is  "get started"
      remote: false,
      networkError: false
    });
  },
},
{
  desc: "Test action=reauth",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield signOut();
  },
  run: function* ()
  {
    const expected_url = "https://example.com/?is_force_auth";
    setPref("identity.fxaccounts.remote.force_auth.uri", expected_url);

    yield setSignedInUser();
    let [, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=reauth");
    // The current user will be appended to the url
    let expected = expected_url + "&email=foo%40example.com";
    is(url, expected, "action=reauth got the expected URL");
  },
},
{
  desc: "Test with migrateToDevEdition enabled (success)",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield signOut();
  },
  run: function* ()
  {
    let fxAccountsCommon = {};
    Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);
    const pref = "identity.fxaccounts.migrateToDevEdition";
    changedPrefs.add(pref);
    Services.prefs.setBoolPref(pref, true);

    // Create the signedInUser.json file that will be used as the source of
    // migrated user data.
    let signedInUser = {
      version: 1,
      accountData: {
        email: "foo@example.com",
        uid: "1234@lcip.org",
        sessionToken: "dead",
        verified: true
      }
    };
    // We use a sub-dir of the real profile dir as the "pretend" profile dir
    // for this test.
    let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let mockDir = profD.clone();
    mockDir.append("about-accounts-mock-profd");
    mockDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    let fxAccountsStorage = OS.Path.join(mockDir.path, fxAccountsCommon.DEFAULT_STORAGE_FILENAME);
    yield OS.File.writeAtomic(fxAccountsStorage, JSON.stringify(signedInUser));
    info("Wrote file " + fxAccountsStorage);

    // this is a little subtle - we load about:robots so we get a non-remote
    // tab, then we send a message which does both (a) load the URL we want and
    // (b) mocks the default profile path used by about:accounts.
    let tab = yield promiseNewTabLoadEvent("about:robots");
    let readyPromise = promiseOneMessage(tab, "test:load-with-mocked-profile-path-response");

    let mm = tab.linkedBrowser.messageManager;
    mm.sendAsyncMessage("test:load-with-mocked-profile-path", {
      url: "about:accounts",
      profilePath: mockDir.path,
    });

    let response = yield readyPromise;
    // We are expecting the iframe to be on the "force reauth" URL
    let expected = yield fxAccounts.promiseAccountsForceSigninURI();
    is(response.data.url, expected);

    let userData = yield fxAccounts.getSignedInUser();
    SimpleTest.isDeeply(userData, signedInUser.accountData, "All account data were migrated");
    // The migration pref will have been switched off by now.
    is(Services.prefs.getBoolPref(pref), false, pref + " got the expected value");

    yield OS.File.remove(fxAccountsStorage);
    yield OS.File.removeEmptyDir(mockDir.path);
  },
},
{
  desc: "Test with migrateToDevEdition enabled (no user to migrate)",
  teardown: function* () {
    gBrowser.removeCurrentTab();
    yield signOut();
  },
  run: function* ()
  {
    const pref = "identity.fxaccounts.migrateToDevEdition";
    changedPrefs.add(pref);
    Services.prefs.setBoolPref(pref, true);

    let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let mockDir = profD.clone();
    mockDir.append("about-accounts-mock-profd");
    mockDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    // but leave it empty, so we don't think a user is logged in.

    let tab = yield promiseNewTabLoadEvent("about:robots");
    let readyPromise = promiseOneMessage(tab, "test:load-with-mocked-profile-path-response");

    let mm = tab.linkedBrowser.messageManager;
    mm.sendAsyncMessage("test:load-with-mocked-profile-path", {
      url: "about:accounts",
      profilePath: mockDir.path,
    });

    let response = yield readyPromise;
    // We are expecting the iframe to be on the "signup" URL
    let expected = yield fxAccounts.promiseAccountsSignUpURI();
    is(response.data.url, expected);

    // and expect no signed in user.
    let userData = yield fxAccounts.getSignedInUser();
    is(userData, null);
    // The migration pref should have still been switched off.
    is(Services.prefs.getBoolPref(pref), false, pref + " got the expected value");
    yield OS.File.removeEmptyDir(mockDir.path);
  },
},
{
  desc: "Test observers about:accounts",
  teardown: function() {
    gBrowser.removeCurrentTab();
  },
  run: function* () {
    setPref("identity.fxaccounts.remote.signup.uri", "https://example.com/");
    yield setSignedInUser();
    let tab = yield promiseNewTabLoadEvent("about:accounts");
    // sign the user out - the tab should have action=signin
    yield signOut();
    // wait for the new load.
    yield promiseOneMessage(tab, "test:document:load");
    is(tab.linkedBrowser.contentDocument.location.href, "about:accounts?action=signin");
  }
},
{
  desc: "Test entrypoint query string, no action, no user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* () {
    // When this loads with no user logged-in, we expect the "normal" URL
    setPref("identity.fxaccounts.remote.signup.uri", "https://example.com/");
    let [, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?entrypoint=abouthome");
    is(url, "https://example.com/?entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "Test entrypoint query string for signin",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* () {
    // When this loads with no user logged-in, we expect the "normal" URL
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    let [, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?action=signin&entrypoint=abouthome");
    is(url, expected_url + "&entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "Test entrypoint query string for signup",
  teardown: () => gBrowser.removeCurrentTab(),
  run: function* () {
    // When this loads with no user logged-in, we expect the "normal" URL
    const sign_up_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", sign_up_url);
    let [, url] = yield promiseNewTabWithIframeLoadEvent("about:accounts?entrypoint=abouthome&action=signup");
    is(url, sign_up_url + "&entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "about:accounts URL params should be copied to remote URL params " +
        "when remote URL has no URL params, except for 'action'",
  teardown() {
    gBrowser.removeCurrentTab();
  },
  run: function* () {
    let signupURL = "https://example.com/";
    setPref("identity.fxaccounts.remote.signup.uri", signupURL);
    let queryStr = "email=foo%40example.com&foo=bar&baz=quux";
    let [, url] =
      yield promiseNewTabWithIframeLoadEvent("about:accounts?" + queryStr +
                                             "&action=action");
    is(url, signupURL + "?" + queryStr, "URL params are copied to signup URL");
  },
},
{
  desc: "about:accounts URL params should be copied to remote URL params " +
        "when remote URL already has some URL params, except for 'action'",
  teardown() {
    gBrowser.removeCurrentTab();
  },
  run: function* () {
    let signupURL = "https://example.com/?param";
    setPref("identity.fxaccounts.remote.signup.uri", signupURL);
    let queryStr = "email=foo%40example.com&foo=bar&baz=quux";
    let [, url] =
      yield promiseNewTabWithIframeLoadEvent("about:accounts?" + queryStr +
                                             "&action=action");
    is(url, signupURL + "&" + queryStr, "URL params are copied to signup URL");
  },
},
]; // gTests

function test()
{
  waitForExplicitFinish();

  Task.spawn(function* () {
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

function promiseOneMessage(tab, messageName) {
  let mm = tab.linkedBrowser.messageManager;
  let deferred = Promise.defer();
  mm.addMessageListener(messageName, function onmessage(message) {
    mm.removeMessageListener(messageName, onmessage);
    deferred.resolve(message);
  });
  return deferred.promise;
}

function promiseNewTabLoadEvent(aUrl)
{
  let tab = gBrowser.selectedTab = gBrowser.addTab(aUrl);
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  // give it an e10s-friendly content script to help with our tests.
  mm.loadFrameScript(CHROME_BASE + "content_aboutAccounts.js", true);
  // and wait for it to tell us about the load.
  return promiseOneMessage(tab, "test:document:load").then(
    () => tab
  );
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

function checkVisibilities(tab, data) {
  let ids = Object.keys(data);
  let mm = tab.linkedBrowser.messageManager;
  let deferred = Promise.defer();
  mm.addMessageListener("test:check-visibilities-response", function onResponse(message) {
    mm.removeMessageListener("test:check-visibilities-response", onResponse);
    for (let id of ids) {
      is(message.data[id], data[id], "Element '" + id + "' has correct visibility");
    }
    deferred.resolve();
  });
  mm.sendAsyncMessage("test:check-visibilities", {ids: ids});
  return deferred.promise;
}

// watch out - these will fire observers which if you aren't careful, may
// interfere with the tests.
function setSignedInUser(data) {
  if (!data) {
    data = {
      email: "foo@example.com",
      uid: "1234@lcip.org",
      assertion: "foobar",
      sessionToken: "dead",
      kA: "beef",
      kB: "cafe",
      verified: true
    }
  }
 return fxAccounts.setSignedInUser(data);
}

function signOut() {
  // we always want a "localOnly" signout here...
  return fxAccounts.signOut(true);
}
