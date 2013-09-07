/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function gc() {
  Cu.forceGC();
  let wu =  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                  .getInterface(Components.interfaces.nsIDOMWindowUtils);
  wu.garbageCollect();
}

// Support for going on and offline.
// (via browser/base/content/test/browser_bookmark_titles.js)
let origProxyType = Services.prefs.getIntPref('network.proxy.type');

function goOffline() {
  // Simulate a network outage with offline mode. (Localhost is still
  // accessible in offline mode, so disable the test proxy as well.)
  if (!Services.io.offline)
    BrowserOffline.toggleOfflineStatus();
  Services.prefs.setIntPref('network.proxy.type', 0);
  // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
  Services.cache.evictEntries(Components.interfaces.nsICache.STORE_ANYWHERE);
}

function goOnline(callback) {
  Services.prefs.setIntPref('network.proxy.type', origProxyType);
  if (Services.io.offline)
    BrowserOffline.toggleOfflineStatus();
  if (callback)
    callback();
}

function openPanel(url, panelCallback, loadCallback) {
  // open a flyout
  SocialFlyout.open(url, 0, panelCallback);
  SocialFlyout.panel.firstChild.addEventListener("load", function panelLoad() {
    SocialFlyout.panel.firstChild.removeEventListener("load", panelLoad, true);
    loadCallback();
  }, true);
}

function openChat(url, panelCallback, loadCallback) {
  // open a chat window
  SocialChatBar.openChat(Social.provider, url, panelCallback);
  SocialChatBar.chatbar.firstChild.addEventListener("DOMContentLoaded", function panelLoad() {
    SocialChatBar.chatbar.firstChild.removeEventListener("DOMContentLoaded", panelLoad, true);
    loadCallback();
  }, true);
}

function onSidebarLoad(callback) {
  let sbrowser = document.getElementById("social-sidebar-browser");
  sbrowser.addEventListener("load", function load() {
    sbrowser.removeEventListener("load", load, true);
    callback();
  }, true);
}

function ensureWorkerLoaded(provider, callback) {
  // once the worker responds to a ping we know it must be up.
  let port = provider.getWorkerPort();
  port.onmessage = function(msg) {
    if (msg.data.topic == "pong") {
      port.close();
      callback();
    }
  }
  port.postMessage({topic: "ping"})
}

let manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
};

function test() {
  waitForExplicitFinish();
  // we don't want the sidebar to auto-load in these tests..
  Services.prefs.setBoolPref("social.sidebar.open", false);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("social.sidebar.open");
  });

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, goOnline, finishcb);
  });
}

var tests = {
  testSidebar: function(next) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    onSidebarLoad(function() {
      ok(sbrowser.contentDocument.location.href.indexOf("about:socialerror?")==0, "is on social error page");
      gc();
      // Add a new load listener, then find and click the "try again" button.
      onSidebarLoad(function() {
        // should still be on the error page.
        ok(sbrowser.contentDocument.location.href.indexOf("about:socialerror?")==0, "is still on social error page");
        // go online and try again - this should work.
        goOnline();
        onSidebarLoad(function() {
          // should now be on the correct page.
          is(sbrowser.contentDocument.location.href, manifest.sidebarURL, "is now on social sidebar page");
          next();
        });
        sbrowser.contentDocument.getElementById("btnTryAgain").click();
      });
      sbrowser.contentDocument.getElementById("btnTryAgain").click();
    });
    // we want the worker to be fully loaded before going offline, otherwise
    // it might fail due to going offline.
    ensureWorkerLoaded(Social.provider, function() {
      // go offline then attempt to load the sidebar - it should fail.
      goOffline();
      Services.prefs.setBoolPref("social.sidebar.open", true);
  });
  },

  testFlyout: function(next) {
    let panelCallbackCount = 0;
    let panel = document.getElementById("social-flyout-panel");
    // go offline and open a flyout.
    goOffline();
    openPanel(
      "https://example.com/browser/browser/base/content/test/social/social_panel.html",
      function() { // the panel api callback
        panelCallbackCount++;
      },
      function() { // the "load" callback.
        executeSoon(function() {
          todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
          ok(panel.firstChild.contentDocument.location.href.indexOf("about:socialerror?")==0, "is on social error page");
          // Bug 832943 - the listeners previously stopped working after a GC, so
          // force a GC now and try again.
          gc();
          openPanel(
            "https://example.com/browser/browser/base/content/test/social/social_panel.html",
            function() { // the panel api callback
              panelCallbackCount++;
            },
            function() { // the "load" callback.
              executeSoon(function() {
                todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
                ok(panel.firstChild.contentDocument.location.href.indexOf("about:socialerror?")==0, "is on social error page");
                gc();
                SocialFlyout.unload();
                next();
              });
            }
          );
        });
      }
    );
  },

  testChatWindow: function(next) {
    let panelCallbackCount = 0;
    // go offline and open a flyout.
    goOffline();
    openChat(
      "https://example.com/browser/browser/base/content/test/social/social_chat.html",
      function() { // the panel api callback
        panelCallbackCount++;
      },
      function() { // the "load" callback.
        executeSoon(function() {
          todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
          let chat = SocialChatBar.chatbar.selectedChat;
          waitForCondition(function() chat.contentDocument.location.href.indexOf("about:socialerror?")==0,
                           function() {
                            chat.close();
                            next();
                            },
                           "error page didn't appear");
        });
      }
    );
  }
}
