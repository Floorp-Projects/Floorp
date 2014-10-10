/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let manifests = [
  {
    name: "provider@example.com",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?example.com",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider@test1",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?test1",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider@test2",
    origin: "https://test2.example.com",
    sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html?test2",
    workerURL: "https://test2.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

let ports = [];
function getProviderPort(provider) {
  let port = provider.getWorkerPort();
  ok(port, "provider has a port");
  ports.push(port);
  return port;
}
let chatId = 0;
function openChat(provider, callback) {
  let chatUrl = provider.origin + "/browser/browser/base/content/test/social/social_chat.html";
  let port = getProviderPort(provider);
  port.onmessage = function(e) {
    if (e.data.topic == "got-chatbox-message") {
      callback();
    }
  }
  let url = chatUrl + "?" + (chatId++);
  port.postMessage({topic: "test-init"});
  port.postMessage({topic: "test-worker-chat", data: url});
  gURLsNotRemembered.push(url);
  return port;
}

function windowHasChats(win) {
  return !!getChatBar().firstElementChild;
}

function test() {
  requestLongerTimeout(2); // only debug builds seem to need more time...
  waitForExplicitFinish();

  let oldwidth = window.outerWidth; // we futz with these, so we restore them
  let oldleft = window.screenX;
  window.moveTo(0, window.screenY)
  let postSubTest = function(cb) {
    // ensure ports are closed
    for (let port of ports) {
      port.close()
      ok(port._closed, "port closed");
    }
    ports = [];

    let chats = document.getElementById("pinnedchats");
    ok(chats.children.length == 0, "no chatty children left behind");
    cb();
  };
  runSocialTestWithProvider(manifests, function (finishcb) {
    ok(Social.enabled, "Social is enabled");
    ok(getProviderPort(Social.providers[0]), "provider 0 has port");
    ok(getProviderPort(Social.providers[1]), "provider 1 has port");
    ok(getProviderPort(Social.providers[2]), "provider 2 has port");
    SocialSidebar.show();
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
    let port = getProviderPort(SocialSidebar.provider);
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
  testWorkerChatWindow: function(next) {
    const chatUrl = SocialSidebar.provider.origin + "/browser/browser/base/content/test/social/social_chat.html";
    let chats = document.getElementById("pinnedchats");
    let port = getProviderPort(SocialSidebar.provider);
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
          next();
          break;
      }
    }
    ok(!chats.selectedChat, "chats are all closed");
    port.postMessage({topic: "test-worker-chat", data: chatUrl});
  },
  testCloseSelf: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = getProviderPort(SocialSidebar.provider);
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
          next();
          break;
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },

  // Check what happens when you close the only visible chat.
  testCloseOnlyVisible: function(next) {
    let chatbar = getChatBar();
    let chatWidth = undefined;
    let num = 0;
    is(chatbar.childNodes.length, 0, "chatbar starting empty");
    is(chatbar.menupopup.childNodes.length, 0, "popup starting empty");
    let port = getProviderPort(SocialSidebar.provider);
    port.postMessage({topic: "test-init"});

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
    let port = getProviderPort(SocialSidebar.provider);
    port.postMessage({topic: "test-init"});
    get3ChatsForCollapsing("normal", function(first, second, third) {
      let chatbar = getChatBar();
      chatbar.showChat(first);
      ok(!first.collapsed, "first should no longer be collapsed");
      is(second.collapsed ||  third.collapsed, true, "one of the others should be collapsed");
      closeAllChats();
      next();
    });
  },

  testOnlyOneCallback: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = getProviderPort(SocialSidebar.provider);
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
            chats.selectedChat.close();
            next();
          });
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },

  testMultipleProviderChat: function(next) {
    // test incomming chats from all providers
    let port0 = openChat(Social.providers[0], function() {
      let port1 = openChat(Social.providers[1], function() {
        let port2 = openChat(Social.providers[2], function() {
          let chats = document.getElementById("pinnedchats");
          waitForCondition(function() chats.children.length == Social.providers.length,
            function() {
              ok(true, "one chat window per provider opened");
              // test logout of a single provider
              port2.postMessage({topic: "test-logout"});
              waitForCondition(function() chats.children.length == Social.providers.length - 1,
                function() {
                  closeAllChats();
                  waitForCondition(function() chats.children.length == 0,
                                   function() {
                                    ok(!chats.selectedChat, "multiprovider chats are all closed");
                                    next();
                                   },
                                   "chat windows didn't close");
                },
                "chat window didn't close");
            }, "chat windows did not open");
        });
      });
    });
  },

  // XXX - note this must be the last test until we restore the login state
  // between tests...
  testCloseOnLogout: function(next) {
    const chatUrl = SocialSidebar.provider.origin + "/browser/browser/base/content/test/social/social_chat.html";
    let port = SocialSidebar.provider.getWorkerPort();
    ports.push(port);
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
    // make sure a user profile is set for this provider as chat windows are
    // only closed on *change* of the profile data rather than merely setting
    // profile data.
    port.postMessage({topic: "test-set-profile"});
    port.postMessage({topic: "test-init"});
  }
}
