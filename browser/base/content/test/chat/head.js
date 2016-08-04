/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utility functions for Chat tests.

var Chat = Cu.import("resource:///modules/Chat.jsm", {}).Chat;
const kDefaultButtonSet = new Set(["minimize", "swap", "close"]);

function promiseOpenChat(url, mode, focus, buttonSet = null) {
  let uri = Services.io.newURI(url, null, null);
  let origin = uri.prePath;
  let title = origin;
  return new Promise(resolve => {
    // we just through a few hoops to ensure the content document is fully
    // loaded, otherwise tests that rely on that content may intermittently fail.
    let callback = function(chatbox) {
      let mm = chatbox.content.messageManager;
      mm.sendAsyncMessage("WaitForDOMContentLoaded");
      mm.addMessageListener("DOMContentLoaded", function cb() {
        mm.removeMessageListener("DOMContentLoaded", cb);
        resolve(chatbox);
      });
    }
    let chatbox = Chat.open(null, {
      origin: origin,
      title: title,
      url: url,
      mode: mode,
      focus: focus
    }, callback);
    if (buttonSet) {
      chatbox.setAttribute("buttonSet", buttonSet);
    }
  });
}

// Opens a chat, returns a promise resolved when the chat callback fired.
function promiseOpenChatCallback(url, mode) {
  let uri = Services.io.newURI(url, null, null);
  let origin = uri.prePath;
  let title = origin;
  return new Promise(resolve => {
    Chat.open(null, { origin, title, url, mode }, resolve);
  });
}

// Opens a chat, returns the chat window's promise which fires when the chat
// starts loading.
function promiseOneEvent(target, eventName, capture) {
  return new Promise(resolve => {
    target.addEventListener(eventName, function handler(event) {
      target.removeEventListener(eventName, handler, capture);
      resolve();
    }, capture);
  });
}

function promiseOneMessage(target, messageName) {
  return new Promise(resolve => {
    let mm = target.messageManager;
    mm.addMessageListener(messageName, function handler() {
      mm.removeMessageListener(messageName, handler);
      resolve();
    });
  });
}

// Return the number of chats in a browser window.
function numChatsInWindow(win) {
  let chatbar = win.document.getElementById("pinnedchats");
  return chatbar.childElementCount;
}

function promiseWaitForFocus() {
  return new Promise(resolve => waitForFocus(resolve));
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

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 100) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function promiseWaitForCondition(aConditionFn) {
  return new Promise((resolve, reject) => {
    waitForCondition(aConditionFn, resolve, "Condition didn't pass.");
  });
}
