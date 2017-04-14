/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/sessionstore/FrameTree.jsm", this);
var gFrameTree = new FrameTree(this);

function executeSoon(callback) {
  Services.tm.mainThread.dispatch(callback, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

gFrameTree.addObserver({
  onFrameTreeReset() {
    sendAsyncMessage("ss-test:onFrameTreeReset");
  },

  onFrameTreeCollected() {
    sendAsyncMessage("ss-test:onFrameTreeCollected");
  }
});

var historyListener = {
  OnHistoryNewEntry() {
    sendAsyncMessage("ss-test:OnHistoryNewEntry");
  },

  OnHistoryGoBack() {
    sendAsyncMessage("ss-test:OnHistoryGoBack");
    return true;
  },

  OnHistoryGoForward() {
    sendAsyncMessage("ss-test:OnHistoryGoForward");
    return true;
  },

  OnHistoryGotoIndex() {
    sendAsyncMessage("ss-test:OnHistoryGotoIndex");
    return true;
  },

  OnHistoryPurge() {
    sendAsyncMessage("ss-test:OnHistoryPurge");
    return true;
  },

  OnHistoryReload() {
    sendAsyncMessage("ss-test:OnHistoryReload");
    return true;
  },

  OnHistoryReplaceEntry() {
    sendAsyncMessage("ss-test:OnHistoryReplaceEntry");
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference
  ])
};

var {sessionHistory} = docShell.QueryInterface(Ci.nsIWebNavigation);
if (sessionHistory) {
  sessionHistory.addSHistoryListener(historyListener);
}

/**
 * This frame script is only loaded for sessionstore mochitests. It enables us
 * to modify and query docShell data when running with multiple processes.
 */

addEventListener("hashchange", function() {
  sendAsyncMessage("ss-test:hashchange");
});

addMessageListener("ss-test:purgeDomainData", function({data: domain}) {
  Services.obs.notifyObservers(null, "browser:purge-domain-data", domain);
  content.setTimeout(() => sendAsyncMessage("ss-test:purgeDomainData"));
});

addMessageListener("ss-test:getStyleSheets", function(msg) {
  let sheets = content.document.styleSheets;
  let titles = Array.map(sheets, ss => [ss.title, ss.disabled]);
  sendSyncMessage("ss-test:getStyleSheets", titles);
});

addMessageListener("ss-test:enableStyleSheetsForSet", function(msg) {
  let sheets = content.document.styleSheets;
  let change = false;
  for (let i = 0; i < sheets.length; i++) {
    if (sheets[i].disabled != (msg.data.indexOf(sheets[i].title) == -1)) {
      change = true;
      break;
    }
  }
  function observer() {
    Services.obs.removeObserver(observer, "style-sheet-applicable-state-changed");

    // It's possible our observer will run before the one in
    // content-sessionStore.js. Therefore, we run ours a little
    // later.
    executeSoon(() => sendAsyncMessage("ss-test:enableStyleSheetsForSet"));
  }
  if (change) {
    // We don't want to reply until content-sessionStore.js has seen
    // the change.
    Services.obs.addObserver(observer, "style-sheet-applicable-state-changed", false);

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
  let {authorStyleDisabled} =
    docShell.contentViewer;
  sendSyncMessage("ss-test:getAuthorStyleDisabled", authorStyleDisabled);
});

addMessageListener("ss-test:setAuthorStyleDisabled", function(msg) {
  let markupDocumentViewer =
    docShell.contentViewer;
  markupDocumentViewer.authorStyleDisabled = msg.data;
  sendSyncMessage("ss-test:setAuthorStyleDisabled");
});

addMessageListener("ss-test:setUsePrivateBrowsing", function(msg) {
  let loadContext =
    docShell.QueryInterface(Ci.nsILoadContext);
  loadContext.usePrivateBrowsing = msg.data;
  sendAsyncMessage("ss-test:setUsePrivateBrowsing");
});

addMessageListener("ss-test:getScrollPosition", function(msg) {
  let frame = content;
  if (msg.data.hasOwnProperty("frame")) {
    frame = content.frames[msg.data.frame];
  }
  let {scrollX: x, scrollY: y} = frame;
  sendAsyncMessage("ss-test:getScrollPosition", {x, y});
});

addMessageListener("ss-test:setScrollPosition", function(msg) {
  let frame = content;
  let {x, y} = msg.data;
  if (msg.data.hasOwnProperty("frame")) {
    frame = content.frames[msg.data.frame];
  }
  frame.scrollTo(x, y);

  frame.addEventListener("scroll", function onScroll(event) {
    if (frame.document == event.target) {
      frame.removeEventListener("scroll", onScroll);
      sendAsyncMessage("ss-test:setScrollPosition");
    }
  });
});

addMessageListener("ss-test:createDynamicFrames", function({data}) {
  function createIFrame(rows) {
    let frames = content.document.getElementById(data.id);
    frames.setAttribute("rows", rows);

    let frame = content.document.createElement("frame");
    frame.setAttribute("src", data.url);
    frames.appendChild(frame);
  }

  addEventListener("DOMContentLoaded", function onContentLoaded(event) {
    if (content.document == event.target) {
      removeEventListener("DOMContentLoaded", onContentLoaded, true);
      // DOMContentLoaded is fired right after we finished parsing the document.
      createIFrame("33%, 33%, 33%");
    }
  }, true);

  addEventListener("load", function onLoad(event) {
    if (content.document == event.target) {
      removeEventListener("load", onLoad, true);

      // Creating this frame on the same tick as the load event
      // means that it must not be included in the frame tree.
      createIFrame("25%, 25%, 25%, 25%");
    }
  }, true);

  sendAsyncMessage("ss-test:createDynamicFrames");
});

addMessageListener("ss-test:removeLastFrame", function({data}) {
  let frames = content.document.getElementById(data.id);
  frames.lastElementChild.remove();
  sendAsyncMessage("ss-test:removeLastFrame");
});

addMessageListener("ss-test:mapFrameTree", function(msg) {
  let result = gFrameTree.map(frame => ({href: frame.location.href}));
  sendAsyncMessage("ss-test:mapFrameTree", result);
});

addMessageListener("ss-test:click", function({data}) {
  content.document.getElementById(data.id).click();
  sendAsyncMessage("ss-test:click");
});

addEventListener("load", function(event) {
  let subframe = event.target != content.document;
  sendAsyncMessage("ss-test:loadEvent", {subframe, url: event.target.documentURI});
}, true);
