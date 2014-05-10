/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let chatbar = document.getElementById("pinnedchats");

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

// Test that chats are created in the correct window.
add_chat_task(function* testChatWindowChooser() {
  let chat = yield promiseOpenChat("http://example.com");
  Assert.equal(numChatsInWindow(window), 1, "first window has the chat");
  // create a second window - this will be the "most recent" and will
  // therefore be the window that hosts the new chat (see bug 835111)
  let secondWindow = OpenBrowserWindow();
  yield promiseOneEvent(secondWindow, "load");
  Assert.equal(numChatsInWindow(secondWindow), 0, "second window starts with no chats");
  yield promiseOpenChat("http://example.com#2");
  Assert.equal(numChatsInWindow(secondWindow), 1, "second window now has chats");
  Assert.equal(numChatsInWindow(window), 1, "first window still has 1 chat");
  chat.close();
  Assert.equal(numChatsInWindow(window), 0, "first window now has no chats");
  // now open another chat - it should still open in the second.
  yield promiseOpenChat("http://example.com#3");
  Assert.equal(numChatsInWindow(window), 0, "first window still has no chats");
  Assert.equal(numChatsInWindow(secondWindow), 2, "second window has both chats");

  // focus the first window, and open yet another chat - it
  // should open in the first window.
  window.focus();
  yield promiseWaitForFocus();
  chat = yield promiseOpenChat("http://example.com#4");
  Assert.equal(numChatsInWindow(window), 1, "first window got new chat");
  chat.close();
  Assert.equal(numChatsInWindow(window), 0, "first window has no chats");

  let privateWindow = OpenBrowserWindow({private: true});
  yield promiseOneEvent(privateWindow, "load")

  // open a last chat - the focused window can't accept
  // chats (it's a private window), so the chat should open
  // in the window that was selected before. This is known
  // to be broken on Linux.
  chat = yield promiseOpenChat("http://example.com#5");
  let os = Services.appinfo.OS;
  const BROKEN_WM_Z_ORDER = os != "WINNT" && os != "Darwin";
  let fn = BROKEN_WM_Z_ORDER ? todo : ok;
  fn(numChatsInWindow(window) == 1, "first window got the chat");
  chat.close();
  privateWindow.close();
  secondWindow.close();
});
