/* globals XPCOMUtils, Cu, Preferences, NewTabWebChannel, is, registerCleanupFunction */

"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabWebChannel",
                                  "resource:///modules/NewTabWebChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabMessages",
                                  "resource:///modules/NewTabMessages.jsm");

const TEST_URL = "https://example.com/browser/browser/components/newtab/tests/browser/newtabwebchannel_basic.html";
const TEST_URL_2 = "http://mochi.test:8888/browser/browser/components/newtab/tests/browser/newtabwebchannel_basic.html";

function setup(mode = "test") {
  Preferences.set("browser.newtabpage.remote.mode", mode);
  Preferences.set("browser.newtabpage.remote", true);
  NewTabWebChannel.init();
  NewTabMessages.init();
}

function cleanup() {
  NewTabMessages.uninit();
  NewTabWebChannel.uninit();
  Preferences.set("browser.newtabpage.remote", false);
  Preferences.set("browser.newtabpage.remote.mode", "production");
}
registerCleanupFunction(cleanup);

/*
 * Tests flow of messages from newtab to chrome and chrome to newtab
 */
add_task(function* open_webchannel_basic() {
  setup();

  let tabOptions = {
    gBrowser,
    url: TEST_URL
  };

  let messagePromise = new Promise(resolve => {
    NewTabWebChannel.once("foo", function(name, msg) {
      is(name, "foo", "Correct message type sent: foo");
      is(msg.data, "bar", "Correct data sent: bar");
      resolve(msg.target);
    });
  });

  let replyPromise = new Promise(resolve => {
    NewTabWebChannel.once("reply", function(name, msg) {
      is(name, "reply", "Correct message type sent: reply");
      is(msg.data, "quuz", "Correct data sent: quuz");
      resolve(msg.target);
    });
  });

  let unloadPromise = new Promise(resolve => {
    NewTabWebChannel.once("targetUnload", function(name) {
      is(name, "targetUnload", "Correct message type sent: targetUnload");
      resolve();
    });
  });

  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield BrowserTestUtils.withNewTab(tabOptions, function*(browser) {
    let target = yield messagePromise;
    is(NewTabWebChannel.numBrowsers, 1, "One target expected");
    is(target.browser, browser, "Same browser");
    NewTabWebChannel.send("respond", null, target);
    yield replyPromise;
  });

  Cu.forceGC();
  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield unloadPromise;
  cleanup();
});

/*
 * Tests message broadcast reaches all open newtab pages
 */
add_task(function* webchannel_broadcast() {
  setup();

  let countingMessagePromise = new Promise(resolve => {
    let count = 0;
    NewTabWebChannel.on("foo", function test_message(name, msg) {
      count += 1;
      if (count === 2) {
        NewTabWebChannel.off("foo", test_message);
        resolve(msg.target);
      }
    });
  });

  let countingReplyPromise = new Promise(resolve => {
    let count = 0;
    NewTabWebChannel.on("reply", function test_message(name, msg) {
      count += 1;
      if (count === 2) {
        NewTabWebChannel.off("reply", test_message);
        resolve(msg.target);
      }
    });
  });

  let countingUnloadPromise = new Promise(resolve => {
    let count = 0;
    NewTabWebChannel.on("targetUnload", function test_message() {
      count += 1;
      if (count === 2) {
        NewTabWebChannel.off("targetUnload", test_message);
        resolve();
      }
    });
  });

  let tabs = [];
  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  tabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL));
  tabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL));

  yield countingMessagePromise;
  is(NewTabWebChannel.numBrowsers, 2, "Two targets expected");

  NewTabWebChannel.broadcast("respond", null);
  yield countingReplyPromise;

  for (let tab of tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  Cu.forceGC();

  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield countingUnloadPromise;
  cleanup();
});

/*
 * Tests switching modes
 */
add_task(function* webchannel_switch() {
  setup();

  function newMessagePromise() {
    return new Promise(resolve => {
      NewTabWebChannel.once("foo", function(name, msg) {
        resolve(msg.target);
      });
    });
  }

  let replyCount = 0;
  function newReplyPromise() {
    return new Promise(resolve => {
      NewTabWebChannel.on("reply", function() {
        replyCount += 1;
        resolve();
      });
    });
  }

  let unloadPromise = new Promise(resolve => {
    NewTabWebChannel.once("targetUnload", function() {
      resolve();
    });
  });

  let unloadAllPromise = new Promise(resolve => {
    NewTabWebChannel.once("targetUnloadAll", function() {
      resolve();
    });
  });

  let tabs = [];
  let messagePromise;
  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");

  messagePromise = newMessagePromise();
  tabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL));
  yield messagePromise;
  is(NewTabWebChannel.numBrowsers, 1, "Correct number of targets");

  messagePromise = newMessagePromise();
  Preferences.set("browser.newtabpage.remote.mode", "test2");
  tabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL_2));
  yield unloadAllPromise;
  yield messagePromise;
  is(NewTabWebChannel.numBrowsers, 1, "Correct number of targets");

  NewTabWebChannel.broadcast("respond", null);
  yield newReplyPromise();
  is(replyCount, 1, "only current channel is listened to for replies");

  const webchannelWhitelistPref = "webchannel.allowObject.urlWhitelist";
  let origWhitelist = Services.prefs.getCharPref(webchannelWhitelistPref);
  let newWhitelist = origWhitelist + " http://mochi.test:8888";
  Services.prefs.setCharPref(webchannelWhitelistPref, newWhitelist);
  try {
    NewTabWebChannel.broadcast("respond_object", null);
    yield newReplyPromise();
  } finally {
    Services.prefs.clearUserPref(webchannelWhitelistPref);
  }

  for (let tab of tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }

  Cu.forceGC();
  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield unloadPromise;
  cleanup();
});

add_task(function* open_webchannel_reload() {
  setup();

  let tabOptions = {
    gBrowser,
    url: TEST_URL
  };

  let messagePromise = new Promise(resolve => {
    NewTabWebChannel.once("foo", function(name, msg) {
      is(name, "foo", "Correct message type sent: foo");
      is(msg.data, "bar", "Correct data sent: bar");
      resolve(msg.target);
    });
  });
  let unloadPromise = new Promise(resolve => {
    NewTabWebChannel.once("targetUnload", function() {
      resolve();
    });
  });

  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield BrowserTestUtils.withNewTab(tabOptions, function*(browser) {
    let target = yield messagePromise;
    is(NewTabWebChannel.numBrowsers, 1, "One target expected");
    is(target.browser, browser, "Same browser");

    browser.reload();
  });

  Cu.forceGC();
  is(NewTabWebChannel.numBrowsers, 0, "Sanity check");
  yield unloadPromise;
  cleanup();
});
