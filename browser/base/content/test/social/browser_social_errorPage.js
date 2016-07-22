/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function gc() {
  Cu.forceGC();
  let wu =  window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                  .getInterface(Components.interfaces.nsIDOMWindowUtils);
  wu.garbageCollect();
}

var openChatWindow = Cu.import("resource://gre/modules/MozSocialAPI.jsm", {}).openChatWindow;

function openPanel(url, panelCallback, loadCallback) {
  // open a flyout
  SocialFlyout.open(url, 0, panelCallback);
  // wait for both open and loaded before callback. Since the test doesn't close
  // the panel between opens, we cannot rely on events here. We need to ensure
  // popupshown happens before we finish out the tests.
  BrowserTestUtils.waitForCondition(function() {
                    return SocialFlyout.panel.state == "open" &&
                           SocialFlyout.iframe.contentDocument.readyState == "complete";
                   },"flyout is open and loaded").then(() => { executeSoon(loadCallback) });
}

function openChat(url, panelCallback, loadCallback) {
  // open a chat window
  let chatbar = getChatBar();
  openChatWindow(null, SocialSidebar.provider, url, panelCallback);
  chatbar.firstChild.addEventListener("DOMContentLoaded", function panelLoad() {
    chatbar.firstChild.removeEventListener("DOMContentLoaded", panelLoad, true);
    executeSoon(loadCallback);
  }, true);
}

function onSidebarLoad(callback) {
  let sbrowser = document.getElementById("social-sidebar-browser");
  sbrowser.addEventListener("load", function load() {
    sbrowser.removeEventListener("load", load, true);
    executeSoon(callback);
  }, true);
}

var manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar_empty.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};

function test() {
  waitForExplicitFinish();

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, function(next) { goOnline().then(next) }, finishcb);
  });
}

var tests = {
  testSidebar: function(next) {
    let sbrowser = document.getElementById("social-sidebar-browser");
    onSidebarLoad(function() {
      ok(sbrowser.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly")==0, "sidebar is on social error page");
      gc();
      // Add a new load listener, then find and click the "try again" button.
      onSidebarLoad(function() {
        // should still be on the error page.
        ok(sbrowser.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly")==0, "sidebar is still on social error page");
        // go online and try again - this should work.
        goOnline().then(function () {
          onSidebarLoad(function() {
            // should now be on the correct page.
            is(sbrowser.contentDocument.documentURI, manifest.sidebarURL, "sidebar is now on social sidebar page");
            next();
          });
          sbrowser.contentDocument.getElementById("btnTryAgain").click();
        });
      });
      sbrowser.contentDocument.getElementById("btnTryAgain").click();
    });
    // go offline then attempt to load the sidebar - it should fail.
    goOffline().then(function() {
      SocialSidebar.show();
    });
  },

  testFlyout: function(next) {
    let panelCallbackCount = 0;
    let panel = document.getElementById("social-flyout-panel");
    goOffline().then(function() {
      openPanel(
        manifest.sidebarURL, /* empty html page */
        function() { // the panel api callback
          panelCallbackCount++;
        },
        function() { // the "load" callback.
          todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
          let href = panel.firstChild.contentDocument.documentURI;
          ok(href.indexOf("about:socialerror?mode=compactInfo")==0, "flyout is on social error page");
          // Bug 832943 - the listeners previously stopped working after a GC, so
          // force a GC now and try again.
          gc();
          openPanel(
            manifest.sidebarURL, /* empty html page */
            function() { // the panel api callback
              panelCallbackCount++;
            },
            function() { // the "load" callback.
              todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
              let href = panel.firstChild.contentDocument.documentURI;
              ok(href.indexOf("about:socialerror?mode=compactInfo")==0, "flyout is on social error page");
              gc();
              SocialFlyout.unload();
              next();
            }
          );
        }
      );
    });
  },

  testChatWindow: function(next) {
    todo(false, "Bug 1245799 is needed to make error pages work again for chat windows.");
    next();
    return;

    let panelCallbackCount = 0;
    // chatwindow tests throw errors, which muddy test output, if the worker
    // doesn't get test-init
    goOffline().then(function() {
      openChat(
        manifest.sidebarURL, /* empty html page */
        function() { // the panel api callback
          panelCallbackCount++;
        },
        function() { // the "load" callback.
          todo_is(panelCallbackCount, 0, "Bug 833207 - should be no callback when error page loads.");
          let chat = getChatBar().selectedChat;
          BrowserTestUtils.waitForCondition(() => chat.content != null && chat.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly")==0,
                           "error page didn't appear").then(() => {
                              chat.close();
                              next();
                            });
        }
      );
    });
  },

  testChatWindowAfterTearOff: function(next) {
    todo(false, "Bug 1245799 is needed to make error pages work again for chat windows.");
    next();
    return;

    // Ensure that the error listener survives the chat window being detached.
    let url = manifest.sidebarURL; /* empty html page */
    let panelCallbackCount = 0;
    // chatwindow tests throw errors, which muddy test output, if the worker
    // doesn't get test-init
    // open a chat while we are still online.
    openChat(
      url,
      null,
      function() { // the "load" callback.
        let chat = getChatBar().selectedChat;
        is(chat.contentDocument.documentURI, url, "correct url loaded");
        // toggle to a detached window.
        chat.swapWindows().then(chat => {
          ok(!!chat.content, "we have chat content 1");
          BrowserTestUtils.waitForCondition(() => chat.content != null && chat.contentDocument.readyState == "complete",
                                            "swapped window loaded").then(() => {
            // now go offline and reload the chat - about:socialerror should be loaded.
            goOffline().then(() => {
              ok(!!chat.content, "we have chat content 2");
              chat.contentDocument.location.reload();
              info("chat reload called");
              BrowserTestUtils.waitForCondition(() => chat.contentDocument.documentURI.indexOf("about:socialerror?mode=tryAgainOnly")==0,
                                                "error page didn't appear").then(() => {
                chat.close();
                next();
              });
            });
          });
        });
      }
    );
  }
}
