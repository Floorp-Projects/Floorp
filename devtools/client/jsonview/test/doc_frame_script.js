/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals sendAsyncMessage, addMessageListener */

// XXX Some helper API could go to:
// testing/mochitest/tests/SimpleTest/AsyncContentUtils.js
// (or at least to share test API in devtools)

/**
 * When the ready state of the JSON View app changes, it triggers custom event
 * "AppReadyStateChange", then the "Test:JsonView:AppReadyStateChange" message
 * will be sent to the parent process for tests to wait for this event if needed.
 */
content.addEventListener("AppReadyStateChange", () => {
  sendAsyncMessage("Test:JsonView:AppReadyStateChange");
});

/**
 * Analogous for the standard "readystatechange" event of the document.
 */
content.document.addEventListener("readystatechange", () => {
  sendAsyncMessage("Test:JsonView:DocReadyStateChange");
});

/**
 * Send a message whenever the server sends a new chunk of JSON data.
 */
new content.MutationObserver(function(mutations, observer) {
  sendAsyncMessage("Test:JsonView:NewDataReceived");
}).observe(content.wrappedJSObject.JSONView.json, { characterData: true });

addMessageListener("Test:JsonView:GetElementCount", function(msg) {
  const { selector } = msg.data;
  const nodeList = content.document.querySelectorAll(selector);
  sendAsyncMessage(msg.name, { count: nodeList.length });
});

addMessageListener("Test:JsonView:GetElementText", function(msg) {
  const { selector } = msg.data;
  const element = content.document.querySelector(selector);
  const text = element ? element.textContent : null;
  sendAsyncMessage(msg.name, { text: text });
});

addMessageListener("Test:JsonView:GetElementVisibleText", function(msg) {
  const { selector } = msg.data;
  const element = content.document.querySelector(selector);
  const text = element ? element.innerText : null;
  sendAsyncMessage(msg.name, { text: text });
});

addMessageListener("Test:JsonView:GetElementAttr", function(msg) {
  const { selector, attr } = msg.data;
  const element = content.document.querySelector(selector);
  const text = element ? element.getAttribute(attr) : null;
  sendAsyncMessage(msg.name, { text: text });
});

addMessageListener("Test:JsonView:FocusElement", function(msg) {
  const { selector } = msg.data;
  const element = content.document.querySelector(selector);
  if (element) {
    element.focus();
  }
  sendAsyncMessage(msg.name);
});

addMessageListener("Test:JsonView:SendString", function(msg) {
  const { selector, str } = msg.data;
  if (selector) {
    const element = content.document.querySelector(selector);
    if (element) {
      element.focus();
    }
  }

  EventUtils.sendString(str, content);

  sendAsyncMessage(msg.name);
});

addMessageListener("Test:JsonView:WaitForFilter", function(msg) {
  const firstRow = content.document.querySelector(
    ".jsonPanelBox .treeTable .treeRow"
  );

  // Check if the filter is already set.
  if (firstRow.classList.contains("hidden")) {
    sendAsyncMessage(msg.name);
    return;
  }

  // Wait till the first row has 'hidden' class set.
  const observer = new content.MutationObserver(function(mutations) {
    for (let i = 0; i < mutations.length; i++) {
      const mutation = mutations[i];
      if (mutation.attributeName == "class") {
        if (firstRow.classList.contains("hidden")) {
          observer.disconnect();
          sendAsyncMessage(msg.name);
          break;
        }
      }
    }
  });

  observer.observe(firstRow, { attributes: true });
});
