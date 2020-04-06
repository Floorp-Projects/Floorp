/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

function executeSoon(callback) {
  Services.tm.dispatchToMainThread(callback);
}

var historyListener = {
  OnHistoryNewEntry() {
    sendAsyncMessage("ss-test:OnHistoryNewEntry");
  },

  OnHistoryGotoIndex() {
    sendAsyncMessage("ss-test:OnHistoryGotoIndex");
  },

  OnHistoryPurge() {
    sendAsyncMessage("ss-test:OnHistoryPurge");
  },

  OnHistoryReload() {
    sendAsyncMessage("ss-test:OnHistoryReload");
    return true;
  },

  OnHistoryReplaceEntry() {
    sendAsyncMessage("ss-test:OnHistoryReplaceEntry");
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference,
  ]),
};

var { sessionHistory } = docShell.QueryInterface(Ci.nsIWebNavigation);
if (sessionHistory) {
  sessionHistory.legacySHistory.addSHistoryListener(historyListener);
}

/**
 * This frame script is only loaded for sessionstore mochitests. It enables us
 * to modify and query docShell data when running with multiple processes.
 */

addEventListener(
  "hashchange",
  function() {
    sendAsyncMessage("ss-test:hashchange");
  },
  true
);

addMessageListener("ss-test:getStyleSheets", function(msg) {
  let sheets = content.document.styleSheets;
  let titles = Array.from(sheets, ss => [ss.title, ss.disabled]);
  sendSyncMessage("ss-test:getStyleSheets", titles);
});

addMessageListener("ss-test:enableStyleSheetsForSet", function(msg) {
  let sheets = content.document.styleSheets;
  let change = false;
  for (let i = 0; i < sheets.length; i++) {
    if (sheets[i].disabled != !msg.data.includes(sheets[i].title)) {
      change = true;
      break;
    }
  }
  function observer() {
    Services.obs.removeObserver(
      observer,
      "style-sheet-applicable-state-changed"
    );

    // It's possible our observer will run before the one in
    // content-sessionStore.js. Therefore, we run ours a little
    // later.
    executeSoon(() => sendAsyncMessage("ss-test:enableStyleSheetsForSet"));
  }
  if (change) {
    // We don't want to reply until content-sessionStore.js has seen
    // the change.
    Services.obs.addObserver(observer, "style-sheet-applicable-state-changed");

    content.document.enableStyleSheetsForSet(msg.data);
  } else {
    sendAsyncMessage("ss-test:enableStyleSheetsForSet");
  }
});

addMessageListener("ss-test:enableSubDocumentStyleSheetsForSet", function(msg) {
  let iframe = content.document.getElementById(msg.data.id);
  iframe.contentDocument.enableStyleSheetsForSet(msg.data.set);
  sendAsyncMessage("ss-test:enableSubDocumentStyleSheetsForSet");
});

addMessageListener("ss-test:getAuthorStyleDisabled", function(msg) {
  let { authorStyleDisabled } = docShell.contentViewer;
  sendSyncMessage("ss-test:getAuthorStyleDisabled", authorStyleDisabled);
});

addMessageListener("ss-test:setAuthorStyleDisabled", function(msg) {
  let markupDocumentViewer = docShell.contentViewer;
  markupDocumentViewer.authorStyleDisabled = msg.data;
  sendSyncMessage("ss-test:setAuthorStyleDisabled");
});

addMessageListener("ss-test:getScrollPosition", function(msg) {
  let frame = content;
  if (msg.data.hasOwnProperty("frame")) {
    frame = content.frames[msg.data.frame];
  }
  let x = {},
    y = {};
  frame.windowUtils.getVisualViewportOffset(x, y);
  sendAsyncMessage("ss-test:getScrollPosition", { x: x.value, y: y.value });
});

addMessageListener("ss-test:setScrollPosition", function(msg) {
  let frame = content;
  let { x, y } = msg.data;
  if (msg.data.hasOwnProperty("frame")) {
    frame = content.frames[msg.data.frame];
  }
  frame.scrollTo(x, y);

  frame.addEventListener(
    "mozvisualscroll",
    function onScroll(event) {
      if (frame.document.ownerGlobal.visualViewport == event.target) {
        frame.removeEventListener("mozvisualscroll", onScroll, {
          mozSystemGroup: true,
        });
        sendAsyncMessage("ss-test:setScrollPosition");
      }
    },
    { mozSystemGroup: true }
  );
});

addMessageListener("ss-test:click", function({ data }) {
  content.document.getElementById(data.id).click();
  sendAsyncMessage("ss-test:click");
});

addEventListener(
  "load",
  function(event) {
    let subframe = event.target != content.document;
    sendAsyncMessage("ss-test:loadEvent", {
      subframe,
      url: event.target.documentURI,
    });
  },
  true
);
