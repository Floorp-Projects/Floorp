/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Is the currently opened tab focused?
function isTabFocused() {
  let tabb = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  return Services.focus.focusedWindow == tabb.contentWindow;
}

function isChatFocused(chat) {
  return SocialChatBar.chatbar._isChatFocused(chat);
}

function openChatViaUser() {
  let sidebarDoc = document.getElementById("social-sidebar-browser").contentDocument;
  let button = sidebarDoc.getElementById("chat-opener");
  // Note we must use synthesizeMouseAtCenter() rather than calling
  // .click() directly as this causes nsIDOMWindowUtils.isHandlingUserInput
  // to be true.
  EventUtils.synthesizeMouseAtCenter(button, {}, sidebarDoc.defaultView);
}

function openChatViaSidebarMessage(port, data, callback) {
  port.onmessage = function (e) {
    if (e.data.topic == "chatbox-opened")
      callback();
  }
  port.postMessage({topic: "test-chatbox-open", data: data});
}

function openChatViaWorkerMessage(port, data, callback) {
  // sadly there is no message coming back to tell us when the chat has
  // been opened, so we wait until one appears.
  let chatbar = SocialChatBar.chatbar;
  let numExpected = chatbar.childElementCount + 1;
  port.postMessage({topic: "test-worker-chat", data: data});
  waitForCondition(function() chatbar.childElementCount == numExpected,
                   function() {
                      // so the child has been added, but we don't know if it
                      // has been intialized - re-request it and the callback
                      // means it's done.  Minimized, same as the worker.
                      SocialChatBar.openChat(Social.provider,
                                             data,
                                             function() {
                                                callback();
                                             },
                                             "minimized");
                   },
                   "No new chat appeared");
}


let isSidebarLoaded = false;

function startTestAndWaitForSidebar(callback) {
  let doneCallback;
  let port = Social.provider.getWorkerPort();
  function maybeCallback() {
    if (!doneCallback)
      callback(port);
    doneCallback = true;
  }
  port.onmessage = function(e) {
    let topic = e.data.topic;
    switch (topic) {
      case "got-sidebar-message":
        // if sidebar loaded too fast, we need a backup ping
      case "got-isVisible-response":
        isSidebarLoaded = true;
        maybeCallback();
        break;
      case "test-init-done":
        if (isSidebarLoaded)
          maybeCallback();
        else
          port.postMessage({topic: "test-isVisible"});
        break;
    }
  }
  port.postMessage({topic: "test-init"});
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

  // Note that (probably) due to bug 604289, if a tab is focused but the
  // focused element is null, our chat windows can "steal" focus.  This is
  // avoided if we explicitly focus an element in the tab.
  // So we load a page with an <input> field and focus that before testing.
  let url = "data:text/html;charset=utf-8," + encodeURI('<input id="theinput">');
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    // before every test we focus the input field.
    let preSubTest = function(cb) {
      // XXX - when bug 604289 is fixed it should be possible to just do:
      // tab.linkedBrowser.contentWindow.focus()
      // but instead we must do:
      tab.linkedBrowser.contentDocument.getElementById("theinput").focus();
      waitForCondition(function() isTabFocused(), cb, "tab should have focus");
    }
    let postSubTest = function(cb) {
      window.SocialChatBar.chatbar.removeAll();
      cb();
    }
    // and run the tests.
    runSocialTestWithProvider(manifest, function (finishcb) {
      runSocialTests(tests, preSubTest, postSubTest, function () {
        finishcb();
      });
    });
  }, true);
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

}

var tests = {
  // In this test the worker asks the sidebar to open a chat.  As that means
  // we aren't handling user-input we will not focus the chatbar.
  // Then we do it again - should still not be focused.
  // Then we perform a user-initiated request - it should get focus.
  testNoFocusWhenViaWorker: function(next) {
    startTestAndWaitForSidebar(function(port) {
      openChatViaSidebarMessage(port, {stealFocus: 1}, function() {
        ok(true, "got chatbox message");
        is(SocialChatBar.chatbar.childElementCount, 1, "exactly 1 chat open");
        ok(isTabFocused(), "tab should still be focused");
        // re-request the same chat via a message.
        openChatViaSidebarMessage(port, {stealFocus: 1}, function() {
          is(SocialChatBar.chatbar.childElementCount, 1, "still exactly 1 chat open");
          ok(isTabFocused(), "tab should still be focused");
          // re-request the same chat via user event.
          openChatViaUser();
          waitForCondition(function() isChatFocused(SocialChatBar.chatbar.selectedChat),
                           function() {
            is(SocialChatBar.chatbar.childElementCount, 1, "still exactly 1 chat open");
            is(SocialChatBar.chatbar.selectedChat, SocialChatBar.chatbar.firstElementChild, "chat should be selected");
            next();
          }, "chat should be focused");
        });
      });
    });
  },

  // In this test we arrange for the sidebar to open the chat via a simulated
  // click.  This should cause the new chat to be opened and focused.
  testFocusWhenViaUser: function(next) {
    startTestAndWaitForSidebar(function(port) {
      openChatViaUser();
      ok(SocialChatBar.chatbar.firstElementChild, "chat opened");
      waitForCondition(function() isChatFocused(SocialChatBar.chatbar.selectedChat),
                       function() {
        is(SocialChatBar.chatbar.selectedChat, SocialChatBar.chatbar.firstElementChild, "chat is selected");
        next();
      }, "chat should be focused");
    });
  },

  // Open a chat via the worker - it will open and not have focus.
  // Then open the same chat via a sidebar message - it will be restored but
  // should still not have grabbed focus.
  testNoFocusOnAutoRestore: function(next) {
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html?id=1";
    let chatbar = SocialChatBar.chatbar;
    startTestAndWaitForSidebar(function(port) {
      openChatViaWorkerMessage(port, chatUrl, function() {
        is(chatbar.childElementCount, 1, "exactly 1 chat open");
        // bug 865086 opening minimized still sets the window as selected
        todo(chatbar.selectedChat != chatbar.firstElementChild, "chat is not selected");
        ok(isTabFocused(), "tab should be focused");
        openChatViaSidebarMessage(port, {stealFocus: 1, id: 1}, function() {
          is(chatbar.childElementCount, 1, "still 1 chat open");
          ok(!chatbar.firstElementChild.minimized, "chat no longer minimized");
          // bug 865086 because we marked it selected on open, it still is
          todo(chatbar.selectedChat != chatbar.firstElementChild, "chat is not selected");
          ok(isTabFocused(), "tab should still be focused");
          next();
        });
      });
    });
  },

  // Here we open a chat, which will not be focused.  Then we minimize it and
  // restore it via a titlebar clock - it should get focus at that point.
  testFocusOnExplicitRestore: function(next) {
    startTestAndWaitForSidebar(function(port) {
      openChatViaSidebarMessage(port, {stealFocus: 1}, function() {
        ok(true, "got chatbox message");
        ok(isTabFocused(), "tab should still be focused");
        let chatbox = SocialChatBar.chatbar.firstElementChild;
        ok(chatbox, "chat opened");
        chatbox.minimized = true;
        ok(isTabFocused(), "tab should still be focused");
        // pretend we clicked on the titlebar
        chatbox.onTitlebarClick({button: 0});
        waitForCondition(function() isChatFocused(SocialChatBar.chatbar.selectedChat),
                         function() {
          ok(!chatbox.minimized, "chat should have been restored");
          ok(isChatFocused(chatbox), "chat should be focused");
          is(chatbox, SocialChatBar.chatbar.selectedChat, "chat is marked selected");
          next();
        }, "chat should have focus");
      });
    });
  },

  // Open 2 chats and give 1 focus.  Minimize the focused one - the second
  // should get focus.
  testMinimizeFocused: function(next) {
    let chatbar = SocialChatBar.chatbar;
    startTestAndWaitForSidebar(function(port) {
      openChatViaSidebarMessage(port, {stealFocus: 1, id: 1}, function() {
        let chat1 = chatbar.firstElementChild;
        openChatViaSidebarMessage(port, {stealFocus: 1, id: 2}, function() {
          is(chatbar.childElementCount, 2, "exactly 2 chats open");
          let chat2 = chat1.nextElementSibling || chat1.previousElementSibling;
          chatbar.selectedChat = chat1;
          chatbar.focus();
          waitForCondition(function() isChatFocused(chat1),
                           function() {
            is(chat1, SocialChatBar.chatbar.selectedChat, "chat1 is marked selected");
            isnot(chat2, SocialChatBar.chatbar.selectedChat, "chat2 is not marked selected");
            chat1.minimized = true;
            waitForCondition(function() isChatFocused(chat2),
                             function() {
              // minimizing the chat with focus should give it to another.
              isnot(chat1, SocialChatBar.chatbar.selectedChat, "chat1 is not marked selected");
              is(chat2, SocialChatBar.chatbar.selectedChat, "chat2 is marked selected");
              next();
            }, "chat2 should have focus");
          }, "chat1 should have focus");
        });
      });
    });
  },

  // Open 2 chats, select (but not focus) one, then re-request it be
  // opened via a message.  Focus should not move.
  testReopenNonFocused: function(next) {
    let chatbar = SocialChatBar.chatbar;
    startTestAndWaitForSidebar(function(port) {
      openChatViaSidebarMessage(port, {id: 1}, function() {
        let chat1 = chatbar.firstElementChild;
        openChatViaSidebarMessage(port, {id: 2}, function() {
          let chat2 = chat1.nextElementSibling || chat1.previousElementSibling;
          chatbar.selectedChat = chat2;
          // tab still has focus
          ok(isTabFocused(), "tab should still be focused");
          // re-request the first.
          openChatViaSidebarMessage(port, {id: 1}, function() {
            is(chatbar.selectedChat, chat1, "chat1 now selected");
            ok(isTabFocused(), "tab should still be focused");
            next();
          });
        });
      });
    });
  },

  // Open 2 chats, select and focus the second.  Pressing the TAB key should
  // cause focus to move between all elements in our chat window before moving
  // to the next chat window.
  testTab: function(next) {
    function sendTabAndWaitForFocus(chat, eltid, callback) {
      // ideally we would use the 'focus' event here, but that doesn't work
      // as expected for the iframe - the iframe itself never gets the focus
      // event (apparently the sub-document etc does.)
      // So just poll for the correct element getting focus...
      let doc = chat.contentDocument;
      EventUtils.sendKey("tab");
      waitForCondition(function() {
        let elt = eltid ? doc.getElementById(eltid) : doc.documentElement;
        return doc.activeElement == elt;
      }, callback, "element " + eltid + " never got focus");
    }

    let chatbar = SocialChatBar.chatbar;
    startTestAndWaitForSidebar(function(port) {
      openChatViaSidebarMessage(port, {id: 1}, function() {
        let chat1 = chatbar.firstElementChild;
        openChatViaSidebarMessage(port, {id: 2}, function() {
          let chat2 = chat1.nextElementSibling || chat1.previousElementSibling;
          chatbar.selectedChat = chat2;
          chatbar.focus();
          waitForCondition(function() isChatFocused(chatbar.selectedChat),
                           function() {
            // Our chats have 3 focusable elements, so it takes 4 TABs to move
            // to the new chat.
            sendTabAndWaitForFocus(chat2, "input1", function() {
              is(chat2.contentDocument.activeElement.getAttribute("id"), "input1",
                 "first input field has focus");
              ok(isChatFocused(chat2), "new chat still focused after first tab");
              sendTabAndWaitForFocus(chat2, "input2", function() {
                ok(isChatFocused(chat2), "new chat still focused after tab");
                is(chat2.contentDocument.activeElement.getAttribute("id"), "input2",
                   "second input field has focus");
                sendTabAndWaitForFocus(chat2, "iframe", function() {
                  ok(isChatFocused(chat2), "new chat still focused after tab");
                  is(chat2.contentDocument.activeElement.getAttribute("id"), "iframe",
                     "iframe has focus");
                  // this tab now should move to the next chat, but focus the
                  // document element itself (hence the null eltid)
                  sendTabAndWaitForFocus(chat1, null, function() {
                    ok(isChatFocused(chat1), "first chat is focused");
                    next();
                  });
                });
              });
            });
          }, "chat should have focus");
        });
      });
    });
  },

  // Open a chat and focus an element other than the first. Move focus to some
  // other item (the tab itself in this case), then focus the chatbar - the
  // same element that was previously focused should still have focus.
  testFocusedElement: function(next) {
    let chatbar = SocialChatBar.chatbar;
    startTestAndWaitForSidebar(function(port) {
      openChatViaUser();
      let chat = chatbar.firstElementChild;
      // need to wait for the content to load before we can focus it.
      chat.addEventListener("DOMContentLoaded", function DOMContentLoaded() {
        chat.removeEventListener("DOMContentLoaded", DOMContentLoaded);
        chat.contentDocument.getElementById("input2").focus();
        waitForCondition(function() isChatFocused(chat),
                         function() {
          is(chat.contentDocument.activeElement.getAttribute("id"), "input2",
             "correct input field has focus");
          // set focus to the tab.
          let tabb = gBrowser.getBrowserForTab(gBrowser.selectedTab);
          Services.focus.moveFocus(tabb.contentWindow, null, Services.focus.MOVEFOCUS_ROOT, 0);
          waitForCondition(function() isTabFocused(),
                           function() {
            chatbar.focus();
            waitForCondition(function() isChatFocused(chat),
                             function() {
              is(chat.contentDocument.activeElement.getAttribute("id"), "input2",
                 "correct input field still has focus");
              next();
            }, "chat took focus");
          }, "tab has focus");
        }, "chat took focus");
      });
    });
  },
};
