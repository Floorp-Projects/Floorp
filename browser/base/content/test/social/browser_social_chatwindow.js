/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  requestLongerTimeout(2); // only debug builds seem to need more time...
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  let oldwidth = window.outerWidth; // we futz with these, so we restore them
  let oldleft = window.screenX;
  window.moveTo(0, window.screenY)
  let postSubTest = function(cb) {
    let chats = document.getElementById("pinnedchats");
    ok(chats.children.length == 0, "no chatty children left behind");
    cb();
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, postSubTest, function() {
      window.moveTo(oldleft, window.screenY)
      window.resizeTo(oldwidth, window.outerHeight);
      finishcb();
    });
  });
}

var tests = {
  testOpenCloseChat: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "got-chatbox-visibility":
          if (e.data.result == "hidden") {
            ok(true, "chatbox got minimized");
            chats.selectedChat.toggle();
          } else if (e.data.result == "shown") {
            ok(true, "chatbox got shown");
            // close it now
            let content = chats.selectedChat.content;
            content.addEventListener("unload", function chatUnload() {
              content.removeEventListener("unload", chatUnload, true);
              ok(true, "got chatbox unload on close");
              port.close();
              next();
            }, true);
            chats.selectedChat.close();
          }
          break;
        case "got-chatbox-message":
          ok(true, "got chatbox message");
          ok(e.data.result == "ok", "got chatbox windowRef result: "+e.data.result);
          chats.selectedChat.toggle();
          break;
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testOpenMinimized: function(next) {
    // In this case the sidebar opens a chat (without specifying minimized).
    // We then minimize it and have the sidebar reopen the chat (again without
    // minimized).  On that second call the chat should open and no longer
    // be minimized.
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    let seen_opened = false;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "chatbox-opened":
          is(e.data.result, "ok", "the sidebar says it got a chatbox");
          if (!seen_opened) {
            // first time we got the opened message, so minimize the chat then
            // re-request the same chat to be opened - we should get the
            // message again and the chat should be restored.
            ok(!chats.selectedChat.minimized, "chat not initially minimized")
            chats.selectedChat.minimized = true
            seen_opened = true;
            port.postMessage({topic: "test-chatbox-open"});
          } else {
            // This is the second time we've seen this message - there should
            // be exactly 1 chat open and it should no longer be minimized.
            let chats = document.getElementById("pinnedchats");
            ok(!chats.selectedChat.minimized, "chat no longer minimized")
            chats.selectedChat.close();
            is(chats.selectedChat, null, "should only have been one chat open");
            port.close();
            next();
          }
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testManyChats: function(next) {
    // open enough chats to overflow the window, then check
    // if the menupopup is visible
    let port = Social.provider.getWorkerPort();
    let chats = document.getElementById("pinnedchats");
    ok(port, "provider has a port");
    ok(chats.menupopup.parentNode.collapsed, "popup nub collapsed at start");
    port.postMessage({topic: "test-init"});
    // we should *never* find a test box that needs more than this to cause
    // an overflow!
    let maxToOpen = 20;
    let numOpened = 0;
    let maybeOpenAnother = function() {
      if (numOpened++ >= maxToOpen) {
        ok(false, "We didn't find a collapsed chat after " + maxToOpen + "chats!");
        closeAllChats();
        next();
      }
      port.postMessage({topic: "test-chatbox-open", data: { id: numOpened }});
    }
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          if (!chats.menupopup.parentNode.collapsed) {
            maybeOpenAnother();
            break;
          }
          ok(true, "popup nub became visible");
          // close our chats now
          while (chats.selectedChat) {
            chats.selectedChat.close();
          }
          ok(!chats.selectedChat, "chats are all closed");
          port.close();
          next();
          break;
      }
    }
    maybeOpenAnother();
  },
  testWorkerChatWindow: function(next) {
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html";
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          ok(true, "got a chat window opened");
          ok(chats.selectedChat, "chatbox from worker opened");
          while (chats.selectedChat) {
            chats.selectedChat.close();
          }
          ok(!chats.selectedChat, "chats are all closed");
          gURLsNotRemembered.push(chatUrl);
          port.close();
          next();
          break;
      }
    }
    ok(!chats.selectedChat, "chats are all closed");
    port.postMessage({topic: "test-worker-chat", data: chatUrl});
  },
  testCloseSelf: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "got-chatbox-visibility":
          is(e.data.result, "shown", "chatbox shown");
          port.close(); // don't want any more visibility messages.
          let chat = chats.selectedChat;
          ok(chat.parentNode, "chat has a parent node before it is closed");
          // ask it to close itself.
          let doc = chat.contentDocument;
          let evt = doc.createEvent("CustomEvent");
          evt.initCustomEvent("socialTest-CloseSelf", true, true, {});
          doc.documentElement.dispatchEvent(evt);
          ok(!chat.parentNode, "chat is now closed");
          port.close();
          next();
          break;
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testSameChatCallbacks: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    let seen_opened = false;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "chatbox-opened":
          is(e.data.result, "ok", "the sidebar says it got a chatbox");
          if (seen_opened) {
            // This is the second time we've seen this message - there should
            // be exactly 1 chat open.
            let chats = document.getElementById("pinnedchats");
            chats.selectedChat.close();
            is(chats.selectedChat, null, "should only have been one chat open");
            port.close();
            next();
          } else {
            // first time we got the opened message, so re-request the same
            // chat to be opened - we should get the message again.
            seen_opened = true;
            port.postMessage({topic: "test-chatbox-open"});
          }
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },

  // check removeAll does the right thing
  testRemoveAll: function(next, mode) {
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});
    get3ChatsForCollapsing(mode || "normal", function() {
      let chatbar = window.SocialChatBar.chatbar;
      chatbar.removeAll();
      // should be no evidence of any chats left.
      is(chatbar.childNodes.length, 0, "should be no chats left");
      checkPopup();
      is(chatbar.selectedChat, null, "nothing should be selected");
      is(chatbar.chatboxForURL.size, 0, "chatboxForURL map should be empty");
      port.close();
      next();
    });
  },

  testRemoveAllMinimized: function(next) {
    this.testRemoveAll(next, "minimized");
  },

  // Check what happens when you close the only visible chat.
  testCloseOnlyVisible: function(next) {
    let chatbar = window.SocialChatBar.chatbar;
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
          closeAllChats();
          next();
        });
      });
    });
  },

  testShowWhenCollapsed: function(next) {
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});
    get3ChatsForCollapsing("normal", function(first, second, third) {
      let chatbar = window.SocialChatBar.chatbar;
      chatbar.showChat(first);
      ok(!first.collapsed, "first should no longer be collapsed");
      ok(second.collapsed ||  third.collapsed, false, "one of the others should be collapsed");
      closeAllChats();
      port.close();
      next();
    });
  },

  testActivity: function(next) {
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});
    get3ChatsForCollapsing("normal", function(first, second, third) {
      let chatbar = window.SocialChatBar.chatbar;
      is(chatbar.selectedChat, third, "third chat should be selected");
      ok(!chatbar.selectedChat.hasAttribute("activity"), "third chat should have no activity");
      // send an activity message to the second.
      ok(!second.hasAttribute("activity"), "second chat should have no activity");
      let chat2 = second.content;
      let evt = chat2.contentDocument.createEvent("CustomEvent");
      evt.initCustomEvent("socialChatActivity", true, true, {});
      chat2.contentDocument.documentElement.dispatchEvent(evt);
      // second should have activity.
      ok(second.hasAttribute("activity"), "second chat should now have activity");
      // select the second - it should lose "activity"
      chatbar.selectedChat = second;
      ok(!second.hasAttribute("activity"), "second chat should no longer have activity");
      // Now try the first - it is collapsed, so the 'nub' also gets activity attr.
      ok(!first.hasAttribute("activity"), "first chat should have no activity");
      let chat1 = first.content;
      let evt = chat1.contentDocument.createEvent("CustomEvent");
      evt.initCustomEvent("socialChatActivity", true, true, {});
      chat1.contentDocument.documentElement.dispatchEvent(evt);
      ok(first.hasAttribute("activity"), "first chat should now have activity");
      ok(chatbar.nub.hasAttribute("activity"), "nub should also have activity");
      // first is collapsed, so use openChat to get it.
      chatbar.openChat(Social.provider, first.getAttribute("src"));
      ok(!first.hasAttribute("activity"), "first chat should no longer have activity");
      // The nub should lose the activity flag here too
      todo(!chatbar.nub.hasAttribute("activity"), "Bug 806266 - nub should no longer have activity");
      // TODO: tests for bug 806266 should arrange to have 2 chats collapsed
      // then open them checking the nub is updated correctly.
      // Now we will go and change the embedded browser in the second chat and
      // ensure the activity magic still works (ie, check that the unload for
      // the browser didn't cause our event handlers to be removed.)
      ok(!second.hasAttribute("activity"), "second chat should have no activity");
      let subiframe = chat2.contentDocument.getElementById("iframe");
      subiframe.contentWindow.addEventListener("unload", function subunload() {
        subiframe.contentWindow.removeEventListener("unload", subunload);
        // ensure all other unload listeners have fired.
        executeSoon(function() {
          let evt = chat2.contentDocument.createEvent("CustomEvent");
          evt.initCustomEvent("socialChatActivity", true, true, {});
          chat2.contentDocument.documentElement.dispatchEvent(evt);
          ok(second.hasAttribute("activity"), "second chat still has activity after unloading sub-iframe");
          closeAllChats();
          port.close();
          next();
        })
      })
      subiframe.setAttribute("src", "data:text/plain:new location for iframe");
    });
  },

  testOnlyOneCallback: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    let numOpened = 0;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "chatbox-opened":
          numOpened += 1;
          port.postMessage({topic: "ping"});
          break;
        case "pong":
          executeSoon(function() {
            is(numOpened, 1, "only got one open message");
            chats.removeAll();
            port.close();
            next();
          });
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },

  testSecondTopLevelWindow: function(next) {
    // Bug 817782 - check chats work in new top-level windows.
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html";
    let port = Social.provider.getWorkerPort();
    let secondWindow;
    port.onmessage = function(e) {
      if (e.data.topic == "test-init-done") {
        secondWindow = OpenBrowserWindow();
        secondWindow.addEventListener("load", function loadListener() {
          secondWindow.removeEventListener("load", loadListener);
          port.postMessage({topic: "test-worker-chat", data: chatUrl});
        });
      } else if (e.data.topic == "got-chatbox-message") {
        // the chat was created - let's make sure it was created in the second window.
        is(secondWindow.SocialChatBar.chatbar.childElementCount, 1);
        secondWindow.close();
        next();
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testChatWindowChooser: function(next) {
    // Tests that when a worker creates a chat, it is opened in the correct
    // window.
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html";
    let chatId = 1;
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});

    function openChat(callback) {
      port.onmessage = function(e) {
        if (e.data.topic == "got-chatbox-message")
          callback();
      }
      let url = chatUrl + "?" + (chatId++);
      port.postMessage({topic: "test-worker-chat", data: url});
    }

    // open a chat (it will open in the main window)
    ok(!window.SocialChatBar.hasChats, "first window should start with no chats");
    openChat(function() {
      ok(window.SocialChatBar.hasChats, "first window has the chat");
      // create a second window - this will be the "most recent" and will
      // therefore be the window that hosts the new chat (see bug 835111)
      let secondWindow = OpenBrowserWindow();
      secondWindow.addEventListener("load", function loadListener() {
        secondWindow.removeEventListener("load", loadListener);
        ok(!secondWindow.SocialChatBar.hasChats, "second window has no chats");
        openChat(function() {
          ok(secondWindow.SocialChatBar.hasChats, "second window now has chats");
          is(window.SocialChatBar.chatbar.childElementCount, 1, "first window still has 1 chat");
          window.SocialChatBar.chatbar.removeAll();
          // now open another chat - it should still open in the second.
          openChat(function() {
            ok(!window.SocialChatBar.hasChats, "first window has no chats");
            ok(secondWindow.SocialChatBar.hasChats, "second window has a chat");
            secondWindow.close();
            port.close();
            next();
          });
        });
      })
    });
  },

  // XXX - note this must be the last test until we restore the login state
  // between tests...
  testCloseOnLogout: function(next) {
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html";
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    let opened = false;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          info("open first chat window");
          port.postMessage({topic: "test-worker-chat", data: chatUrl});
          break;
        case "got-chatbox-message":
          ok(true, "got a chat window opened");
          if (opened) {
            port.postMessage({topic: "test-logout"});
            waitForCondition(function() document.getElementById("pinnedchats").firstChild == null,
                             function() {
                              port.close();
                              next();
                             },
                             "chat windows didn't close");
          } else {
            // open a second chat window
            opened = true;
            port.postMessage({topic: "test-worker-chat", data: chatUrl+"?id=1"});
          }
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  }
}
