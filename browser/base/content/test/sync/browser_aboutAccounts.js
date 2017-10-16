/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

const CHROME_BASE = "chrome://mochitests/content/browser/browser/base/content/test/sync/";
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
  desc: "Test action=signin - no user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  async run() {
    // When this loads with no user logged-in, we expect the "normal" URL
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    let [tab, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    is(url, expected_url, "action=signin got the expected URL");
    // we expect the remote iframe to be shown.
    await checkVisibilities(tab, {
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
  async teardown() {
    gBrowser.removeCurrentTab();
    await signOut();
  },
  async run() {
    // When this loads with a user logged-in, we expect the normal URL to
    // have been ignored and the "manage" page to be shown.
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    await setSignedInUser();
    let tab = await promiseNewTabLoadEvent("about:accounts?action=signin");
    // about:accounts initializes after fetching the current user from Fxa -
    // so we also request it - by the time we get it we know it should have
    // done its thing.
    await fxAccounts.getSignedInUser();
    // we expect "manage" to be shown.
    await checkVisibilities(tab, {
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
  async run() {
    const signinUrl = "https://redirproxy.example.com/test";
    setPref("identity.fxaccounts.remote.signin.uri", signinUrl);
    let [tab, ] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    await checkVisibilities(tab, {
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
  async run() {
    BrowserOffline.toggleOfflineStatus();
    Services.cache2.clear();

    const signinUrl = "https://unknowndomain.cow";
    setPref("identity.fxaccounts.remote.signin.uri", signinUrl);
    let [tab, ] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=signin");
    await checkVisibilities(tab, {
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
  async run() {
    const expected_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", expected_url);
    let [tab, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=signup");
    is(url, expected_url, "action=signup got the expected URL");
    // we expect the remote iframe to be shown.
    await checkVisibilities(tab, {
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
  async run() {
    const expected_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", expected_url);
    await setSignedInUser();
    let tab = await promiseNewTabLoadEvent("about:accounts?action=signup");
    await fxAccounts.getSignedInUser();
    // we expect "manage" to be shown.
    await checkVisibilities(tab, {
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
  async teardown() {
    gBrowser.removeCurrentTab();
    await signOut();
  },
  async run() {
    const expected_url = "https://example.com/force_auth";
    setPref("identity.fxaccounts.remote.force_auth.uri", expected_url);

    await setSignedInUser();
    let [, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=reauth");
    // The current user will be appended to the url
    let expected = expected_url + "?uid=1234%40lcip.org&email=foo%40example.com";
    is(url, expected, "action=reauth got the expected URL");
  },
},
{
  desc: "Test with migrateToDevEdition enabled (success)",
  async teardown() {
    gBrowser.removeCurrentTab();
    await signOut();
  },
  async run() {
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
    await OS.File.writeAtomic(fxAccountsStorage, JSON.stringify(signedInUser));
    info("Wrote file " + fxAccountsStorage);

    // this is a little subtle - we load about:robots so we get a non-remote
    // tab, then we send a message which does both (a) load the URL we want and
    // (b) mocks the default profile path used by about:accounts.
    let tab = await promiseNewTabLoadEvent("about:robots");
    let readyPromise = promiseOneMessage(tab, "test:load-with-mocked-profile-path-response");

    let mm = tab.linkedBrowser.messageManager;
    mm.sendAsyncMessage("test:load-with-mocked-profile-path", {
      url: "about:accounts",
      profilePath: mockDir.path,
    });

    let response = await readyPromise;
    // We are expecting the iframe to be on the "force reauth" URL
    let expected = await fxAccounts.promiseAccountsForceSigninURI();
    is(response.data.url, expected);

    let userData = await fxAccounts.getSignedInUser();
    SimpleTest.isDeeply(userData, signedInUser.accountData, "All account data were migrated");
    // The migration pref will have been switched off by now.
    is(Services.prefs.getBoolPref(pref), false, pref + " got the expected value");

    await OS.File.remove(fxAccountsStorage);
    await OS.File.removeEmptyDir(mockDir.path);
  },
},
{
  desc: "Test with migrateToDevEdition enabled (no user to migrate)",
  async teardown() {
    gBrowser.removeCurrentTab();
    await signOut();
  },
  async run() {
    const pref = "identity.fxaccounts.migrateToDevEdition";
    changedPrefs.add(pref);
    Services.prefs.setBoolPref(pref, true);

    let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let mockDir = profD.clone();
    mockDir.append("about-accounts-mock-profd");
    mockDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    // but leave it empty, so we don't think a user is logged in.

    let tab = await promiseNewTabLoadEvent("about:robots");
    let readyPromise = promiseOneMessage(tab, "test:load-with-mocked-profile-path-response");

    let mm = tab.linkedBrowser.messageManager;
    mm.sendAsyncMessage("test:load-with-mocked-profile-path", {
      url: "about:accounts",
      profilePath: mockDir.path,
    });

    let response = await readyPromise;
    // We are expecting the iframe to be on the "signup" URL
    let expected = await fxAccounts.promiseAccountsSignUpURI();
    is(response.data.url, expected);

    // and expect no signed in user.
    let userData = await fxAccounts.getSignedInUser();
    is(userData, null);
    // The migration pref should have still been switched off.
    is(Services.prefs.getBoolPref(pref), false, pref + " got the expected value");
    await OS.File.removeEmptyDir(mockDir.path);
  },
},
{
  desc: "Test observers about:accounts",
  teardown() {
    gBrowser.removeCurrentTab();
  },
  async run() {
    setPref("identity.fxaccounts.remote.signup.uri", "https://example.com/");
    await setSignedInUser();
    let tab = await promiseNewTabLoadEvent("about:accounts");
    // sign the user out - the tab should have action=signin
    let loadPromise = promiseOneMessage(tab, "test:document:load");
    await signOut();
    // wait for the new load.
    await loadPromise;
    is(tab.linkedBrowser.contentDocument.location.href, "about:accounts?action=signin");
  }
},
{
  desc: "Test entrypoint query string, no action, no user logged in",
  teardown: () => gBrowser.removeCurrentTab(),
  async run() {
    // When this loads with no user logged-in, we expect the "normal" URL
    setPref("identity.fxaccounts.remote.signup.uri", "https://example.com/");
    let [, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?entrypoint=abouthome");
    is(url, "https://example.com/?entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "Test entrypoint query string for signin",
  teardown: () => gBrowser.removeCurrentTab(),
  async run() {
    // When this loads with no user logged-in, we expect the "normal" URL
    const expected_url = "https://example.com/?is_sign_in";
    setPref("identity.fxaccounts.remote.signin.uri", expected_url);
    let [, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?action=signin&entrypoint=abouthome");
    is(url, expected_url + "&entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "Test entrypoint query string for signup",
  teardown: () => gBrowser.removeCurrentTab(),
  async run() {
    // When this loads with no user logged-in, we expect the "normal" URL
    const sign_up_url = "https://example.com/?is_sign_up";
    setPref("identity.fxaccounts.remote.signup.uri", sign_up_url);
    let [, url] = await promiseNewTabWithIframeLoadEvent("about:accounts?entrypoint=abouthome&action=signup");
    is(url, sign_up_url + "&entrypoint=abouthome", "entrypoint=abouthome got the expected URL");
  },
},
{
  desc: "about:accounts URL params should be copied to remote URL params " +
        "when remote URL has no URL params, except for 'action'",
  teardown() {
    gBrowser.removeCurrentTab();
  },
  async run() {
    let signupURL = "https://example.com/";
    setPref("identity.fxaccounts.remote.signup.uri", signupURL);
    let queryStr = "email=foo%40example.com&foo=bar&baz=quux";
    let [, url] =
      await promiseNewTabWithIframeLoadEvent("about:accounts?" + queryStr +
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
  async run() {
    let signupURL = "https://example.com/?param";
    setPref("identity.fxaccounts.remote.signup.uri", signupURL);
    let queryStr = "email=foo%40example.com&foo=bar&baz=quux";
    let [, url] =
      await promiseNewTabWithIframeLoadEvent("about:accounts?" + queryStr +
                                             "&action=action");
    is(url, signupURL + "&" + queryStr, "URL params are copied to signup URL");
  },
},
]; // gTests

function test() {
  waitForExplicitFinish();

  (async function() {
    for (let testCase of gTests) {
      info(testCase.desc);
      try {
        await testCase.run();
      } finally {
        await testCase.teardown();
      }
    }

    finish();
  })();
}

function promiseOneMessage(tab, messageName) {
  let mm = tab.linkedBrowser.messageManager;
  return new Promise(resolve => {
    mm.addMessageListener(messageName, function onmessage(message) {
      mm.removeMessageListener(messageName, onmessage);
      resolve(message);
    });
  });
}

function promiseNewTabLoadEvent(aUrl) {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, aUrl);
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  // give it an e10s-friendly content script to help with our tests,
  // and wait for it to tell us about the load.
  let promise = promiseOneMessage(tab, "test:document:load");
  mm.loadFrameScript(CHROME_BASE + "content_aboutAccounts.js", true);
  return promise.then(() => tab);
}

// Returns a promise which is resolved with the iframe's URL after a new
// tab is created and the iframe in that tab loads.
function promiseNewTabWithIframeLoadEvent(aUrl) {
  return new Promise(resolve => {
    let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, aUrl);
    let browser = tab.linkedBrowser;
    let mm = browser.messageManager;

    // give it an e10s-friendly content script to help with our tests,
    // and wait for it to tell us about the iframe load.
    mm.addMessageListener("test:iframe:load", function onFrameLoad(message) {
      mm.removeMessageListener("test:iframe:load", onFrameLoad);
      resolve([tab, message.data.url]);
    });
    mm.loadFrameScript(CHROME_BASE + "content_aboutAccounts.js", true);
  });
}

function checkVisibilities(tab, data) {
  let ids = Object.keys(data);
  let mm = tab.linkedBrowser.messageManager;
  return new Promise(resolve => {
    mm.addMessageListener("test:check-visibilities-response", function onResponse(message) {
      mm.removeMessageListener("test:check-visibilities-response", onResponse);
      for (let id of ids) {
        is(message.data[id], data[id], "Element '" + id + "' has correct visibility");
      }
      resolve();
    });
    mm.sendAsyncMessage("test:check-visibilities", {ids});
  });
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
    };
  }
 return fxAccounts.setSignedInUser(data);
}

function signOut() {
  // we always want a "localOnly" signout here...
  return fxAccounts.signOut(true);
}
