/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      let chats = document.getElementById("pinnedchats");
      ok(chats.children.length == 0, "no chatty children left behind");
      finishcb();
    });
  });
}

var tests = {
  testOpenCloseChat: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.port;
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
            let iframe = chats.selectedChat.iframe;
            iframe.addEventListener("unload", function chatUnload() {
              iframe.removeEventListener("unload", chatUnload, true);
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
    port.postMessage({topic: "test-init"});
  },
  testManyChats: function(next) {
    // open enough chats to overflow the window, then check
    // if the menupopup is visible
    let port = Social.provider.port;
    ok(port, "provider has a port");
    let width = document.documentElement.boxObject.width;
    let numToOpen = (width / 200) + 1;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          numToOpen--;
          if (numToOpen >= 0) {
            // we're waiting for all to open
            ok(true, "got a chat window opened");
            break;
          }
          // close our chats now
          let chats = document.getElementById("pinnedchats");
          ok(!chats.menupopup.parentNode.collapsed, "menu selection is visible");
          while (chats.selectedChat) {
            chats.selectedChat.close();
          }
          ok(!chats.selectedChat, "chats are all closed");
          next();
          break;
      }
    }
    let num = numToOpen;
    while (num-- > 0) {
      port.postMessage({topic: "test-chatbox-open"});
    }
  }
}

