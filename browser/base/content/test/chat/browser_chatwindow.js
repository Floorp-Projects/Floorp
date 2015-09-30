/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Promise.jsm", this);

var chatbar = document.getElementById("pinnedchats");

add_chat_task(function* testOpenCloseChat() {
  let chatbox = yield promiseOpenChat("http://example.com");
  Assert.strictEqual(chatbox, chatbar.selectedChat);
  // we requested a "normal" chat, so shouldn't be minimized
  Assert.ok(!chatbox.minimized, "chat is not minimized");
  Assert.equal(chatbar.childNodes.length, 1, "should be 1 chat open");


  // now request the same URL again - we should get the same chat.
  let chatbox2 = yield promiseOpenChat("http://example.com");
  Assert.strictEqual(chatbox2, chatbox, "got the same chat");
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");

  chatbox.toggle();
  is(chatbox.minimized, true, "chat is now minimized");
  // was no other chat to select, so selected becomes null.
  is(chatbar.selectedChat, null);

  // We check the content gets an unload event as we close it.
  let promiseClosed = promiseOneEvent(chatbox.content, "unload", true);
  chatbox.close();
  yield promiseClosed;
});

// In this case we open a chat minimized, then request the same chat again
// without specifying minimized.  On that second call the chat should open,
// selected, and no longer minimized.
add_chat_task(function* testMinimized() {
  let chatbox = yield promiseOpenChat("http://example.com", "minimized");
  Assert.strictEqual(chatbox, chatbar.selectedChat);
  Assert.ok(chatbox.minimized, "chat is minimized");
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");
  yield promiseOpenChat("http://example.com");
  Assert.ok(!chatbox.minimized, false, "chat is no longer minimized");
});

// open enough chats to overflow the window, then check
// if the menupopup is visible
add_chat_task(function* testManyChats() {
  Assert.ok(chatbar.menupopup.parentNode.collapsed, "popup nub collapsed at start");
  // we should *never* find a test box that needs more than this to cause
  // an overflow!
  let maxToOpen = 20;
  let numOpened = 0;
  for (let i = 0; i < maxToOpen; i++) {
    yield promiseOpenChat("http://example.com#" + i);
    if (!chatbar.menupopup.parentNode.collapsed) {
      info("the menu popup appeared");
      return;
    }
  }
  Assert.ok(false, "We didn't find a collapsed chat after " + maxToOpen + "chats!");
});

// Check that closeAll works as expected.
add_chat_task(function* testOpenTwiceCallbacks() {
  yield promiseOpenChat("http://example.com#1");
  yield promiseOpenChat("http://example.com#2");
  yield promiseOpenChat("http://test2.example.com");
  Assert.equal(numChatsInWindow(window), 3, "should be 3 chats open");
  Chat.closeAll("http://example.com");
  Assert.equal(numChatsInWindow(window), 1, "should have closed 2 chats");
  Chat.closeAll("http://test2.example.com");
  Assert.equal(numChatsInWindow(window), 0, "should have closed last chat");
});

// Check that when we open the same chat twice, the callbacks are called back
// twice.
add_chat_task(function* testOpenTwiceCallbacks() {
  yield promiseOpenChatCallback("http://example.com");
  yield promiseOpenChatCallback("http://example.com");
});

// Bug 817782 - check chats work in new top-level windows.
add_chat_task(function* testSecondTopLevelWindow() {
  const chatUrl = "http://example.com";
  let secondWindow = OpenBrowserWindow();
  yield promiseOneEvent(secondWindow, "load");
  yield promiseOpenChat(chatUrl);
  // the chat was created - let's make sure it was created in the second window.
  Assert.equal(numChatsInWindow(window), 0, "main window has no chats");
  Assert.equal(numChatsInWindow(secondWindow), 1, "second window has 1 chat");
  secondWindow.close();
});

// Test that findChromeWindowForChats() returns the expected window.
add_chat_task(function* testChatWindowChooser() {
  let chat = yield promiseOpenChat("http://example.com");
  Assert.equal(numChatsInWindow(window), 1, "first window has the chat");
  // create a second window - this will be the "most recent" and will
  // therefore be the window that hosts the new chat (see bug 835111)
  let secondWindow = OpenBrowserWindow();
  yield promiseOneEvent(secondWindow, "load");
  Assert.equal(secondWindow, Chat.findChromeWindowForChats(null), "Second window is the preferred chat window");

  // focus the first window, and check it will be selected for future chats.
  // Bug 1090633 - there are lots of issues around focus, especially when the
  // browser itself doesn't have focus. We can end up with
  // Services.wm.getMostRecentWindow("navigator:browser") returning a different
  // window than, say, Services.focus.activeWindow. But the focus manager isn't
  // the point of this test.
  // So we simply keep focusing the first window until it is reported as the
  // most recent.
  do {
    dump("trying to force window to become the most recent.\n");
    secondWindow.focus();
    window.focus();
    yield promiseWaitForFocus();
  } while (Services.wm.getMostRecentWindow("navigator:browser") != window)

  Assert.equal(window, Chat.findChromeWindowForChats(null), "First window now the preferred chat window");

  let privateWindow = OpenBrowserWindow({private: true});
  yield promiseOneEvent(privateWindow, "load")

  // The focused window can't accept chats (it's a private window), so the
  // chat should open in the window that was selected before. This will be
  // either window or secondWindow (linux may choose a different one) but the
  // point is that the window is *not* the private one.
  Assert.ok(Chat.findChromeWindowForChats(null) == window ||
            Chat.findChromeWindowForChats(null) == secondWindow,
            "Private window isn't selected for new chats.");
  privateWindow.close();
  secondWindow.close();
});

add_chat_task(function* testButtonSet() {
  let chatbox = yield promiseOpenChat("http://example.com#1");
  let document = chatbox.ownerDocument;

  // Expect all default buttons to be visible.
  for (let buttonId of kDefaultButtonSet) {
    let button = document.getAnonymousElementByAttribute(chatbox, "anonid", buttonId);
    Assert.ok(!button.hidden, "Button '" + buttonId + "' should be visible");
  }

  let visible = new Set(["minimize", "close"]);
  chatbox = yield promiseOpenChat("http://example.com#2", null, null, [...visible].join(","));

  for (let buttonId of kDefaultButtonSet) {
    let button = document.getAnonymousElementByAttribute(chatbox, "anonid", buttonId);
    if (visible.has(buttonId)) {
      Assert.ok(!button.hidden, "Button '" + buttonId + "' should be visible");
    } else {
      Assert.ok(button.hidden, "Button '" + buttonId + "' should NOT be visible");
    }
  }
});

add_chat_task(function* testCustomButton() {
  let commanded = 0;
  let customButton = {
    id: "custom",
    onCommand: function() {
      ++commanded;
    }
  };

  Chat.registerButton(customButton);

  let chatbox = yield promiseOpenChat("http://example.com#1");
  let document = chatbox.ownerDocument;
  let titlebarNode = document.getAnonymousElementByAttribute(chatbox, "class",
    "chat-titlebar");

  Assert.equal(titlebarNode.getElementsByClassName("chat-custom")[0], null,
    "Custom chat button should not be in the toolbar yet.");

  let visible = new Set(["minimize", "close", "custom"]);
  Chat.loadButtonSet(chatbox, [...visible].join(","));

  for (let buttonId of kDefaultButtonSet) {
    let button = document.getAnonymousElementByAttribute(chatbox, "anonid", buttonId);
    if (visible.has(buttonId)) {
      Assert.ok(!button.hidden, "Button '" + buttonId + "' should be visible");
    } else {
      Assert.ok(button.hidden, "Button '" + buttonId + "' should NOT be visible");
    }
  }

  let customButtonNode = titlebarNode.getElementsByClassName("chat-custom")[0];
  Assert.ok(!customButtonNode.hidden, "Custom button should be visible");

  let ev = document.createEvent("XULCommandEvent");
  ev.initCommandEvent("command", true, true, document.defaultView, 0, false,
    false, false, false, null);
  customButtonNode.dispatchEvent(ev);

  Assert.equal(commanded, 1, "Button should have been commanded once");
});
