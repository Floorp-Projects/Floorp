/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

var manifests = [
  {
    name: "provider@example.com",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?example.com",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider@test1",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?test1",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider@test2",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html?test2",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var chatId = 0;
function openChat(provider) {
  return new Promise(resolve => {
    SocialSidebar.provider = provider;
    let chatUrl = provider.origin + "/browser/browser/base/content/test/social/social_chat.html";
    let url = chatUrl + "?id=" + (chatId++);
    makeChat("normal", "chat " + chatId, (cb) => { resolve(cb); });
  });
}

function windowHasChats(win) {
  return !!getChatBar().firstElementChild;
}

function test() {
  requestLongerTimeout(2); // only debug builds seem to need more time...
  waitForExplicitFinish();

  let frameScript = "data:,(" + function frame_script() {
    addMessageListener("socialTest-CloseSelf", function(e) {
      content.close();
    }, true);
  }.toString() + ")();";
  let mm = getGroupMessageManager("social");
  mm.loadFrameScript(frameScript, true);

  let oldwidth = window.outerWidth; // we futz with these, so we restore them
  let oldleft = window.screenX;
  window.moveTo(0, window.screenY)
  let postSubTest = function(cb) {
    let chats = document.getElementById("pinnedchats");
    ok(chats.children.length == 0, "no chatty children left behind");
    cb();
  };
  runSocialTestWithProvider(manifests, function (finishcb) {
    ok(Social.enabled, "Social is enabled");
    SocialSidebar.show();
    runSocialTests(tests, undefined, postSubTest, function() {
      window.moveTo(oldleft, window.screenY)
      window.resizeTo(oldwidth, window.outerHeight);
      mm.removeDelayedFrameScript(frameScript);
      finishcb();
    });
  });
}

var tests = {
  testOpenCloseChat: function(next) {
    openChat(SocialSidebar.provider).then((cb) => {
      BrowserTestUtils.waitForCondition(() => { return cb.minimized; },
                                        "chatbox is minimized").then(() => {
        ok(cb.minimized, "chat is minimized after toggle");
        BrowserTestUtils.waitForCondition(() => { return !cb.minimized; },
                                          "chatbox is not minimized").then(() => {
          ok(!cb.minimized, "chat is not minimized after toggle");
          promiseNodeRemoved(cb).then(next);
          let mm = cb.content.messageManager;
          mm.sendAsyncMessage("socialTest-CloseSelf", {});
          info("close chat window requested");
        });
        cb.toggle();
      });

      ok(!cb.minimized, "chat is not minimized on open");
      // toggle to minimize chat
      cb.toggle();
    });
  },

  // Check what happens when you close the only visible chat.
  testCloseOnlyVisible: function(next) {
    let chatbar = getChatBar();
    let chatWidth = undefined;
    let num = 0;
    is(chatbar.childNodes.length, 0, "chatbar starting empty");
    is(chatbar.menupopup.childNodes.length, 0, "popup starting empty");

    makeChat("normal", "first chat", function() {
      // got the first one.
      checkPopup();
      ok(chatbar.menupopup.parentNode.collapsed, "menu selection isn't visible");
      // we kinda cheat here and get the width of the first chat, assuming
      // that all future chats will have the same width when open.
      chatWidth = chatbar.calcTotalWidthOf(chatbar.selectedChat);
      let desired = chatWidth * 1.5;
      resizeWindowToChatAreaWidth(desired, function(sizedOk) {
        ok(sizedOk, "can't do any tests without this width");
        checkPopup();
        makeChat("normal", "second chat", function() {
          is(chatbar.childNodes.length, 2, "now have 2 chats");
          let first = chatbar.childNodes[0];
          let second = chatbar.childNodes[1];
          is(chatbar.selectedChat, first, "first chat is selected");
          ok(second.collapsed, "second chat is currently collapsed");
          // closing the first chat will leave enough room for the second
          // chat to appear, and thus become selected.
          chatbar.selectedChat.close();
          is(chatbar.selectedChat, second, "second chat is selected");
          Task.spawn(closeAllChats).then(next);
        });
      });
    });
  },

  testShowWhenCollapsed: function(next) {
    get3ChatsForCollapsing("normal", function(first, second, third) {
      let chatbar = getChatBar();
      chatbar.showChat(first);
      ok(!first.collapsed, "first should no longer be collapsed");
      is(second.collapsed ||  third.collapsed, true, "one of the others should be collapsed");
      Task.spawn(closeAllChats).then(next);
    });
  }
}
