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

  let postSubTest = function(cb) {
    let chats = document.getElementById("pinnedchats");
    ok(chats.children.length == 0, "no chatty children left behind");
    cb();
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, postSubTest, function() {
      finishcb();
    });
  });
}

var tests = {
  testTearoffChat: function(next) {
    let chats = document.getElementById("pinnedchats");
    let chatTitle;
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "got-chatbox-visibility":
          // chatbox is open, lets detach. The new chat window will be caught in
          // the window watcher below
          let doc = chats.selectedChat.contentDocument;
          // This message is (sometimes!) received a second time
          // before we start our tests from the onCloseWindow
          // callback.
          if (doc.location == "about:blank")
            return;
          chatTitle = doc.title;
          ok(chats.selectedChat.getAttribute("label") == chatTitle,
             "the new chatbox should show the title of the chat window");
          let div = doc.createElement("div");
          div.setAttribute("id", "testdiv");
          div.setAttribute("test", "1");
          doc.body.appendChild(div);
          let swap = document.getAnonymousElementByAttribute(chats.selectedChat, "anonid", "swap");
          swap.click();
          port.close();
          break;
        case "got-chatbox-message":
          ok(true, "got chatbox message");
          ok(e.data.result == "ok", "got chatbox windowRef result: "+e.data.result);
          chats.selectedChat.toggle();
          break;
      }
    }

    Services.wm.addListener({
      onWindowTitleChange: function() {},
      onCloseWindow: function(xulwindow) {},
      onOpenWindow: function(xulwindow) {
        var domwindow = xulwindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIDOMWindow);
        Services.wm.removeListener(this);
        // wait for load to ensure the window is ready for us to test
        domwindow.addEventListener("load", function _load(event) {
          let doc = domwindow.document;
          if (event.target != doc)
              return;

          domwindow.removeEventListener("load", _load, false);

          domwindow.addEventListener("unload", function _close(event) {
            if (event.target != doc)
              return;
            domwindow.removeEventListener("unload", _close, false);
            info("window has been closed");
            waitForCondition(function() {
              return chats.selectedChat && chats.selectedChat.contentDocument &&
                     chats.selectedChat.contentDocument.readyState == "complete";
            },function () {
              ok(chats.selectedChat, "should have a chatbox in our window again");
              ok(chats.selectedChat.getAttribute("label") == chatTitle,
                 "the new chatbox should show the title of the chat window again");
              let testdiv = chats.selectedChat.contentDocument.getElementById("testdiv");
              is(testdiv.getAttribute("test"), "2", "docshell should have been swapped");
              chats.selectedChat.close();
              waitForCondition(function() {
                return chats.children.length == 0;
              },function () {
                next();
              });
            });
          }, false);

          is(doc.documentElement.getAttribute("windowtype"), "Social:Chat", "Social:Chat window opened");
          // window is loaded, but the docswap does not happen until after load,
          // and we have no event to wait on, so we'll wait for document state
          // to be ready
          let chatbox = doc.getElementById("chatter");
          waitForCondition(function() {
            return chats.selectedChat == null &&
                   chatbox.contentDocument &&
                   chatbox.contentDocument.readyState == "complete";
          },function() {
            ok(chatbox.getAttribute("label") == chatTitle,
               "detached window should show the title of the chat window");
            let testdiv = chatbox.contentDocument.getElementById("testdiv");
            is(testdiv.getAttribute("test"), "1", "docshell should have been swapped");
            testdiv.setAttribute("test", "2");
            // swap the window back to the chatbar
            let swap = doc.getAnonymousElementByAttribute(chatbox, "anonid", "swap");
            swap.click();
          }, domwindow);
        }, false);
      }
    });

    port.postMessage({topic: "test-init", data: { id: 1 }});
  },

  testCloseOnLogout: function(next) {
    let chats = document.getElementById("pinnedchats");
    const chatUrl = "https://example.com/browser/browser/base/content/test/social/social_chat.html";
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-visibility":
          // chatbox is open, lets detach. The new chat window will be caught in
          // the window watcher below
          let doc = chats.selectedChat.contentDocument;
          // This message is (sometimes!) received a second time
          // before we start our tests from the onCloseWindow
          // callback.
          if (doc.location == "about:blank")
            return;
          info("chatbox is open, detach from window");
          let swap = document.getAnonymousElementByAttribute(chats.selectedChat, "anonid", "swap");
          swap.click();
          break;
      }
    }

    Services.wm.addListener({
      onWindowTitleChange: function() {},
      onCloseWindow: function(xulwindow) {},
      onOpenWindow: function(xulwindow) {
        let domwindow = xulwindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                              .getInterface(Components.interfaces.nsIDOMWindow);
        Services.wm.removeListener(this);
        // wait for load to ensure the window is ready for us to test, make sure
        // we're not getting called for about:blank
        domwindow.addEventListener("load", function _load(event) {
          let doc = domwindow.document;
          if (event.target != doc)
              return;
          domwindow.removeEventListener("load", _load, false);

          domwindow.addEventListener("unload", function _close(event) {
            if (event.target != doc)
              return;
            domwindow.removeEventListener("unload", _close, false);
            ok(true, "window has been closed");
            next();
          }, false);

          is(doc.documentElement.getAttribute("windowtype"), "Social:Chat", "Social:Chat window opened");
          // window is loaded, but the docswap does not happen until after load,
          // and we have no event to wait on, so we'll wait for document state
          // to be ready
          let chatbox = doc.getElementById("chatter");
          waitForCondition(function() {
            return chats.children.length == 0 &&
                   chatbox.contentDocument &&
                   chatbox.contentDocument.readyState == "complete";
          },function() {
            // logout, we should get unload next
            port.postMessage({topic: "test-logout"});
            port.close();
          }, domwindow);

        }, false);
      }
    });

    port.postMessage({topic: "test-worker-chat", data: chatUrl});
  },
}