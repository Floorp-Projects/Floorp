/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function isChatFocused(chat) {
  return getChatBar()._isChatFocused(chat);
}

var manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};

function test() {
  waitForExplicitFinish();

  // Note that (probably) due to bug 604289, if a tab is focused but the
  // focused element is null, our chat windows can "steal" focus.  This is
  // avoided if we explicitly focus an element in the tab.
  // So we load a page with an <input> field and focus that before testing.
  let url = "data:text/html;charset=utf-8," + encodeURI('<input id="theinput">');
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  browser.addEventListener("load", function tabLoad(event) {
    browser.removeEventListener("load", tabLoad, true);
    // before every test we focus the input field.
    let preSubTest = function(cb) {
      ContentTask.spawn(browser, null, function* () {
        content.focus();
        content.document.getElementById("theinput").focus();

        yield ContentTaskUtils.waitForCondition(
          () => Services.focus.focusedWindow == content, "tab should have focus");
      }).then(cb);
    }
    let postSubTest = function(cb) {
      Task.spawn(closeAllChats).then(cb);
    }
    // and run the tests.
    runSocialTestWithProvider(manifest, function (finishcb) {
      SocialSidebar.show();
      runSocialTests(tests, preSubTest, postSubTest, function () {
        BrowserTestUtils.removeTab(tab).then(finishcb);
      });
    });
  }, true);
}

var tests = {
  // In this test we arrange for the sidebar to open the chat via a simulated
  // click.  This should cause the new chat to be opened and focused.
  testFocusWhenViaUser: function(next) {
    ensureFrameLoaded(document.getElementById("social-sidebar-browser")).then(() => {
      let chatbar = getChatBar();
      openChatViaUser();
      ok(chatbar.firstElementChild, "chat opened");
      BrowserTestUtils.waitForCondition(() => isChatFocused(chatbar.selectedChat),
                                        "chat should be focused").then(() => {
        is(chatbar.selectedChat, chatbar.firstElementChild, "chat is selected");
        next();
      });
    });
  },
};
