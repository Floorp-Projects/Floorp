/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

const FRAME_URI = "chrome://mochitests/content/browser/dom/base/test/" +
                  "disableNonTestMouseEvents-frame.js";
const TEST_URI = "http://example.com/browser/dom/base/test/" +
                 "disableNonTestMouseEvents.html";
const TYPES = ["click", "mousedown", "mousemove", "mouseover", "mouseup"];

EventUtils.disableNonTestMouseEvents(true);

let triggered = false;

function addTab(url) {
  window.focus();

  let tabSwitchPromise = new Promise((resolve, reject) => {
    window.gBrowser.addEventListener("TabSwitchDone", function listener() {
      window.gBrowser.removeEventListener("TabSwitchDone", listener);
      resolve();
    });
  });

  let loadPromise = new Promise(function(resolve, reject) {
    let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onload() {
      linkedBrowser.removeEventListener("load", onload, true);
      resolve(tab);
    }, true);
  });

  return Promise.all([tabSwitchPromise, loadPromise]);
}

function triggerEvent(type) {
  let mm = gBrowser.selectedBrowser.messageManager;

  return new Promise(function(resolve, reject) {
    let handler = function() {
      info("in triggerEvent handler");
      triggered = true;
      mm.removeMessageListener("Test:EventReply", handler);
      resolve();
    };

    mm.addMessageListener("Test:EventReply", handler);
    info("sending synthesize message: " + type);
    mm.sendAsyncMessage("Test:SynthesizeEvent", type);
  });
}

add_task(function*() {
  for (let type of TYPES) {
    triggered = false;

    info("adding new tab");
    yield addTab(TEST_URI);

    let mm = gBrowser.selectedBrowser.messageManager;
    info("sending frame script");
    mm.loadFrameScript(FRAME_URI, false);

    yield triggerEvent(type);

    ok(triggered, type + " event was triggered");

    gBrowser.removeCurrentTab();
  }
});
