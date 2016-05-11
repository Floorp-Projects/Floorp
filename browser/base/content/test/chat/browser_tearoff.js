/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var chatbar = document.getElementById("pinnedchats");

function promiseNewWindowLoaded() {
  return new Promise(resolve => {
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
          domwindow.removeEventListener("load", _load);
          resolve(domwindow);
        });
      },
    });
  });
}

add_chat_task(function* testTearoffChat() {
  let chatbox = yield promiseOpenChat("http://example.com");
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");

  let chatTitle = yield ContentTask.spawn(chatbox.content, null, function* () {
    let chatDoc = content.document;

    // Mutate the chat document a bit before we tear it off.
    let div = chatDoc.createElement("div");
    div.setAttribute("id", "testdiv");
    div.setAttribute("test", "1");
    chatDoc.body.appendChild(div);

    return chatDoc.title;
  });

  Assert.equal(chatbox.getAttribute("label"), chatTitle,
               "the new chatbox should show the title of the chat window");

  // chatbox is open, lets detach. The new chat window will be caught in
  // the window watcher below
  let promise = promiseNewWindowLoaded();

  let swap = document.getAnonymousElementByAttribute(chatbox, "anonid", "swap");
  swap.click();

  // and wait for the new window.
  let domwindow = yield promise;

  Assert.equal(domwindow.document.documentElement.getAttribute("windowtype"), "Social:Chat", "Social:Chat window opened");
  Assert.equal(numChatsInWindow(window), 0, "should be no chats in the chat bar");

  // get the chatbox from the new window.
  chatbox = domwindow.document.getElementById("chatter")
  Assert.equal(chatbox.getAttribute("label"), chatTitle, "window should have same title as chat");

  yield ContentTask.spawn(chatbox.content, null, function* () {
    let div = content.document.getElementById("testdiv");
    Assert.equal(div.getAttribute("test"), "1", "docshell should have been swapped");
    div.setAttribute("test", "2");
  });

  // swap the window back to the chatbar
  promise = promiseOneEvent(domwindow, "unload");
  swap = domwindow.document.getAnonymousElementByAttribute(chatbox, "anonid", "swap");
  swap.click();

  yield promise;

  Assert.equal(numChatsInWindow(window), 1, "chat should be docked back in the window");
  chatbox = chatbar.selectedChat;
  Assert.equal(chatbox.getAttribute("label"), chatTitle,
               "the new chatbox should show the title of the chat window again");

  yield ContentTask.spawn(chatbox.content, null, function* () {
    let div = content.document.getElementById("testdiv");
    Assert.equal(div.getAttribute("test"), "2", "docshell should have been swapped");
  });
});

// Similar test but with 2 chats.
add_chat_task(function* testReattachTwice() {
  let chatbox1 = yield promiseOpenChat("http://example.com#1");
  let chatbox2 = yield promiseOpenChat("http://example.com#2");
  Assert.equal(numChatsInWindow(window), 2, "both chats should be docked in the window");

  info("chatboxes are open, detach from window");
  let promise = promiseNewWindowLoaded();
  document.getAnonymousElementByAttribute(chatbox1, "anonid", "swap").click();
  let domwindow1 = yield promise;
  chatbox1 = domwindow1.document.getElementById("chatter");
  Assert.equal(numChatsInWindow(window), 1, "only second chat should be docked in the window");

  promise = promiseNewWindowLoaded();
  document.getAnonymousElementByAttribute(chatbox2, "anonid", "swap").click();
  let domwindow2 = yield promise;
  chatbox2 = domwindow2.document.getElementById("chatter");
  Assert.equal(numChatsInWindow(window), 0, "should be no docked chats");

  promise = promiseOneEvent(domwindow2, "unload");
  domwindow2.document.getAnonymousElementByAttribute(chatbox2, "anonid", "swap").click();
  yield promise;
  Assert.equal(numChatsInWindow(window), 1, "one chat should be docked back in the window");

  promise = promiseOneEvent(domwindow1, "unload");
  domwindow1.document.getAnonymousElementByAttribute(chatbox1, "anonid", "swap").click();
  yield promise;
  Assert.equal(numChatsInWindow(window), 2, "both chats should be docked back in the window");
});

// Check that Chat.closeAll() also closes detached windows.
add_chat_task(function* testCloseAll() {
  let chatbox1 = yield promiseOpenChat("http://example.com#1");
  let chatbox2 = yield promiseOpenChat("http://example.com#2");

  let promise = promiseNewWindowLoaded();
  document.getAnonymousElementByAttribute(chatbox1, "anonid", "swap").click();
  let domwindow = yield promise;
  chatbox1 = domwindow.document.getElementById("chatter");

  let promiseWindowUnload = promiseOneEvent(domwindow, "unload");

  Assert.equal(numChatsInWindow(window), 1, "second chat should still be docked");
  Chat.closeAll("http://example.com");
  yield promiseWindowUnload;
  Assert.equal(numChatsInWindow(window), 0, "should be no chats left");
});
