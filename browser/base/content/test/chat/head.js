/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions for Chat tests.

let Chat = Cu.import("resource:///modules/Chat.jsm", {}).Chat;

function promiseOpenChat(url, mode, focus) {
  let uri = Services.io.newURI(url, null, null);
  let origin = uri.prePath;
  let title = origin;
  let deferred = Promise.defer();
  // we just through a few hoops to ensure the content document is fully
  // loaded, otherwise tests that rely on that content may intermittently fail.
  let callback = function(chatbox) {
    if (chatbox.contentDocument.readyState == "complete") {
      // already loaded.
      deferred.resolve(chatbox);
      return;
    }
    chatbox.addEventListener("load", function onload(event) {
      if (event.target != chatbox.contentDocument || chatbox.contentDocument.location.href == "about:blank") {
        return;
      }
      chatbox.removeEventListener("load", onload, true);
      deferred.resolve(chatbox);
    }, true);
  }
  let chatbox = Chat.open(null, origin, title, url, mode, focus, callback);
  return deferred.promise;
}

// Opens a chat, returns a promise resolved when the chat callback fired.
function promiseOpenChatCallback(url, mode) {
  let uri = Services.io.newURI(url, null, null);
  let origin = uri.prePath;
  let title = origin;
  let deferred = Promise.defer();
  let callback = deferred.resolve;
  Chat.open(null, origin, title, url, mode, undefined, callback);
  return deferred.promise;
}

// Opens a chat, returns the chat window's promise which fires when the chat
// starts loading.
function promiseOneEvent(target, eventName, capture) {
  let deferred = Promise.defer();
  target.addEventListener(eventName, function handler(event) {
    target.removeEventListener(eventName, handler, capture);
    deferred.resolve();
  }, capture);
  return deferred.promise;
}

// Return the number of chats in a browser window.
function numChatsInWindow(win) {
  let chatbar = win.document.getElementById("pinnedchats");
  return chatbar.childElementCount;
}

function promiseWaitForFocus() {
  let deferred = Promise.defer();
  waitForFocus(deferred.resolve);
  return deferred.promise;
}

// A simple way to clean up after each test.
function add_chat_task(genFunction) {
  add_task(function* () {
    info("Starting chat test " + genFunction.name);
    try {
      yield genFunction();
    } finally {
      info("Finished chat test " + genFunction.name + " - cleaning up.");
      // close all docked chats.
      while (chatbar.childNodes.length) {
        chatbar.childNodes[0].close();
      }
      // and non-docked chats.
      let winEnum = Services.wm.getEnumerator("Social:Chat");
      while (winEnum.hasMoreElements()) {
        let win = winEnum.getNext();
        if (win.closed) {
          continue;
        }
        win.close();
      }
    }
  });
}
