/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals Services, sendAsyncMessage, addMessageListener */

// XXX Some helper API could go to:
// testing/mochitest/tests/SimpleTest/AsyncContentUtils.js
// (or at least to share test API in devtools)

// Set up a dummy environment so that EventUtils works. We need to be careful to
// pass a window object into each EventUtils method we call rather than having
// it rely on the |window| global.
let EventUtils = {};
EventUtils.window = content;
EventUtils.parent = EventUtils.window;
EventUtils._EU_Ci = Components.interfaces; // eslint-disable-line
EventUtils._EU_Cc = Components.classes; // eslint-disable-line
EventUtils.navigator = content.navigator;
EventUtils.KeyboardEvent = content.KeyboardEvent;

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

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

addMessageListener("Test:JsonView:GetElementCount", function (msg) {
  let {selector} = msg.data;
  let nodeList = content.document.querySelectorAll(selector);
  sendAsyncMessage(msg.name, {count: nodeList.length});
});

addMessageListener("Test:JsonView:GetElementText", function (msg) {
  let {selector} = msg.data;
  let element = content.document.querySelector(selector);
  let text = element ? element.textContent : null;
  sendAsyncMessage(msg.name, {text: text});
});

addMessageListener("Test:JsonView:GetElementVisibleText", function (msg) {
  let {selector} = msg.data;
  let element = content.document.querySelector(selector);
  let text = element ? element.innerText : null;
  sendAsyncMessage(msg.name, {text: text});
});

addMessageListener("Test:JsonView:GetElementAttr", function (msg) {
  let {selector, attr} = msg.data;
  let element = content.document.querySelector(selector);
  let text = element ? element.getAttribute(attr) : null;
  sendAsyncMessage(msg.name, {text: text});
});

addMessageListener("Test:JsonView:FocusElement", function (msg) {
  let {selector} = msg.data;
  let element = content.document.querySelector(selector);
  if (element) {
    element.focus();
  }
  sendAsyncMessage(msg.name);
});

addMessageListener("Test:JsonView:SendString", function (msg) {
  let {selector, str} = msg.data;
  if (selector) {
    let element = content.document.querySelector(selector);
    if (element) {
      element.focus();
    }
  }

  EventUtils.sendString(str, content);

  sendAsyncMessage(msg.name);
});

addMessageListener("Test:JsonView:WaitForFilter", function (msg) {
  let firstRow = content.document.querySelector(
    ".jsonPanelBox .treeTable .treeRow");

  // Check if the filter is already set.
  if (firstRow.classList.contains("hidden")) {
    sendAsyncMessage(msg.name);
    return;
  }

  // Wait till the first row has 'hidden' class set.
  let observer = new content.MutationObserver(function (mutations) {
    for (let i = 0; i < mutations.length; i++) {
      let mutation = mutations[i];
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

addMessageListener("Test:JsonView:Eval", function (msg) {
  let result = content.eval(msg.data.code);
  sendAsyncMessage(msg.name, {result});
});

Components.utils.exportFunction(content.document.querySelector.bind(content.document),
  content, {defineAs: "$"});
Components.utils.exportFunction(content.document.querySelectorAll.bind(content.document),
  content, {defineAs: "$$"});
