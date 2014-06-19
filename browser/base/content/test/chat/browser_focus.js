/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the focus functionality.

const CHAT_URL = "https://example.com/browser/browser/base/content/test/chat/chat.html";

// Is the currently opened tab focused?
function isTabFocused() {
  let tabb = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  return Services.focus.focusedWindow == tabb.contentWindow;
}

// Is the specified chat focused?
function isChatFocused(chat) {
  return chat.chatbar._isChatFocused(chat);
}

let chatbar = document.getElementById("pinnedchats");

function* setUp() {
  // Note that (probably) due to bug 604289, if a tab is focused but the
  // focused element is null, our chat windows can "steal" focus.  This is
  // avoided if we explicitly focus an element in the tab.
  // So we load a page with an <input> field and focus that before testing.
  let html = '<input id="theinput"><button id="chat-opener"></button>';
  let url = "data:text/html;charset=utf-8," + encodeURI(html);
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  yield promiseOneEvent(tab.linkedBrowser, "load", true);
  tab.linkedBrowser.contentDocument.getElementById("theinput").focus();
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });
}

// Test default focus - not user input.
add_chat_task(function* testDefaultFocus() {
  yield setUp();
  let chat = yield promiseOpenChat("http://example.com");
  // we used the default focus behaviour, which means that because this was
  // not the direct result of user action the chat should not be focused.
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");
  Assert.ok(isTabFocused(), "the tab should remain focused.");
  Assert.ok(!isChatFocused(chat), "the chat should not be focused.");
});

// Test default focus via user input.
add_chat_task(function* testDefaultFocus() {
  yield setUp();
  let tab = gBrowser.selectedTab;
  let deferred = Promise.defer();
  let button = tab.linkedBrowser.contentDocument.getElementById("chat-opener");
  button.addEventListener("click", function onclick() {
    button.removeEventListener("click", onclick);
    promiseOpenChat("http://example.com").then(
      chat => deferred.resolve(chat)
    );
  })
  // Note we must use synthesizeMouseAtCenter() rather than calling
  // .click() directly as this causes nsIDOMWindowUtils.isHandlingUserInput
  // to be true.
  EventUtils.synthesizeMouseAtCenter(button, {}, button.ownerDocument.defaultView);
  let chat = yield deferred.promise;

  // we use the default focus behaviour but the chat was opened via user input,
  // so the chat should be focused.
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");
  Assert.ok(!isTabFocused(), "the tab should have lost focus.");
  Assert.ok(isChatFocused(chat), "the chat should have got focus.");
});

// We explicitly ask for the chat to be focused.
add_chat_task(function* testExplicitFocus() {
  yield setUp();
  let chat = yield promiseOpenChat("http://example.com", undefined, true);
  // we use the default focus behaviour, which means that because this was
  // not the direct result of user action the chat should not be focused.
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");
  Assert.ok(!isTabFocused(), "the tab should have lost focus.");
  Assert.ok(isChatFocused(chat), "the chat should have got focus.");
});

// Open a minimized chat via default focus behaviour - it will open and not
// have focus.  Then open the same chat without 'minimized' - it will be
// restored but should still not have grabbed focus.
add_chat_task(function* testNoFocusOnAutoRestore() {
  yield setUp();
  let chat = yield promiseOpenChat("http://example.com", "minimized");
  Assert.ok(chat.minimized, "chat is minimized");
  Assert.equal(numChatsInWindow(window), 1, "should be 1 chat open");
  Assert.ok(isTabFocused(), "the tab should remain focused.");
  Assert.ok(!isChatFocused(chat), "the chat should not be focused.");
  yield promiseOpenChat("http://example.com");
  Assert.ok(!chat.minimized, "chat should be restored");
  Assert.ok(isTabFocused(), "the tab should remain focused.");
  Assert.ok(!isChatFocused(chat), "the chat should not be focused.");
});

// Here we open a chat, which will not be focused.  Then we minimize it and
// restore it via a titlebar clock - it should get focus at that point.
add_chat_task(function* testFocusOnExplicitRestore() {
  yield setUp();
  let chat = yield promiseOpenChat("http://example.com");
  Assert.ok(!chat.minimized, "chat should have been opened restored");
  Assert.ok(isTabFocused(), "the tab should remain focused.");
  Assert.ok(!isChatFocused(chat), "the chat should not be focused.");
  chat.minimized = true;
  Assert.ok(isTabFocused(), "tab should still be focused");
  Assert.ok(!isChatFocused(chat), "the chat should not be focused.");

  let promise = promiseOneEvent(chat.contentWindow, "focus");
  // pretend we clicked on the titlebar
  chat.onTitlebarClick({button: 0});
  yield promise; // wait for focus event.
  Assert.ok(!chat.minimized, "chat should have been restored");
  Assert.ok(isChatFocused(chat), "chat should be focused");
  Assert.strictEqual(chat, chatbar.selectedChat, "chat is marked selected");
});

// Open 2 chats and give 1 focus.  Minimize the focused one - the second
// should get focus.
add_chat_task(function* testMinimizeFocused() {
  yield setUp();
  let chat1 = yield promiseOpenChat("http://example.com#1");
  let chat2 = yield promiseOpenChat("http://example.com#2");
  Assert.equal(numChatsInWindow(window), 2, "2 chats open");
  Assert.strictEqual(chatbar.selectedChat, chat2, "chat2 is selected");
  let promise = promiseOneEvent(chat1.contentWindow, "focus");
  chatbar.selectedChat = chat1;
  chatbar.focus();
  yield promise; // wait for chat1 to get focus.
  Assert.strictEqual(chat1, chatbar.selectedChat, "chat1 is marked selected");
  Assert.notStrictEqual(chat2, chatbar.selectedChat, "chat2 is not marked selected");
  promise = promiseOneEvent(chat2.contentWindow, "focus");
  chat1.minimized = true;
  yield promise; // wait for chat2 to get focus.
  Assert.notStrictEqual(chat1, chatbar.selectedChat, "chat1 is not marked selected");
  Assert.strictEqual(chat2, chatbar.selectedChat, "chat2 is marked selected");
});

// Open 2 chats, select and focus the second.  Pressing the TAB key should
// cause focus to move between all elements in our chat window before moving
// to the next chat window.
add_chat_task(function* testTab() {
  yield setUp();

  function sendTabAndWaitForFocus(chat, eltid) {
    let doc = chat.contentDocument;
    EventUtils.sendKey("tab");
    // ideally we would use the 'focus' event here, but that doesn't work
    // as expected for the iframe - the iframe itself never gets the focus
    // event (apparently the sub-document etc does.)
    // So just poll for the correct element getting focus...
    let deferred = Promise.defer();
    let tries = 0;
    let interval = setInterval(function() {
      if (tries >= 30) {
        clearInterval(interval);
        deferred.reject("never got focus");
        return;
      }
      tries ++;
      let elt = eltid ? doc.getElementById(eltid) : doc.documentElement;
      if (doc.activeElement == elt) {
        clearInterval(interval);
        deferred.resolve();
      }
      info("retrying wait for focus: " + tries);
      info("(the active element is " + doc.activeElement + "/" + doc.activeElement.getAttribute("id") + ")");
    }, 100);
    info("waiting for element " + eltid + " to get focus");
    return deferred.promise;
  }

  let chat1 = yield promiseOpenChat(CHAT_URL + "#1");
  let chat2 = yield promiseOpenChat(CHAT_URL + "#2");
  chatbar.selectedChat = chat2;
  let promise = promiseOneEvent(chat2.contentWindow, "focus");
  chatbar.focus();
  info("waiting for second chat to get focus");
  yield promise;

  // Our chats have 3 focusable elements, so it takes 4 TABs to move
  // to the new chat.
  yield sendTabAndWaitForFocus(chat2, "input1");
  Assert.equal(chat2.contentDocument.activeElement.getAttribute("id"), "input1",
               "first input field has focus");
  Assert.ok(isChatFocused(chat2), "new chat still focused after first tab");

  yield sendTabAndWaitForFocus(chat2, "input2");
  Assert.ok(isChatFocused(chat2), "new chat still focused after tab");
  Assert.equal(chat2.contentDocument.activeElement.getAttribute("id"), "input2",
               "second input field has focus");

  yield sendTabAndWaitForFocus(chat2, "iframe");
  Assert.ok(isChatFocused(chat2), "new chat still focused after tab");
  Assert.equal(chat2.contentDocument.activeElement.getAttribute("id"), "iframe",
               "iframe has focus");

  // this tab now should move to the next chat, but focus the
  // document element itself (hence the null eltid)
  yield sendTabAndWaitForFocus(chat1, null);
  Assert.ok(isChatFocused(chat1), "first chat is focused");
});

// Open a chat and focus an element other than the first. Move focus to some
// other item (the tab itself in this case), then focus the chatbar - the
// same element that was previously focused should still have focus.
add_chat_task(function* testFocusedElement() {
  yield setUp();

  // open a chat with focus requested.
  let chat = yield promiseOpenChat(CHAT_URL, undefined, true);

  chat.contentDocument.getElementById("input2").focus();

  // set focus to the tab.
  let tabb = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  let promise = promiseOneEvent(tabb.contentWindow, "focus");
  Services.focus.moveFocus(tabb.contentWindow, null, Services.focus.MOVEFOCUS_ROOT, 0);
  yield promise;

  promise = promiseOneEvent(chat.contentWindow, "focus");
  chatbar.focus();
  yield promise;

  Assert.equal(chat.contentDocument.activeElement.getAttribute("id"), "input2",
               "correct input field still has focus");
});
