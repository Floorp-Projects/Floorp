/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported recordInitialTimestamps onlyNewItemsFilter checkRecentlyClosed */

let initialTimestamps = [];

function recordInitialTimestamps(timestamps) {
  initialTimestamps = timestamps;
}

function onlyNewItemsFilter(item) {
  return !initialTimestamps.includes(item.lastModified);
}

function checkWindow(window) {
  for (let prop of ["focused", "incognito", "alwaysOnTop"]) {
    is(window[prop], false, `closed window has the expected value for ${prop}`);
  }
  for (let prop of ["state", "type"]) {
    is(window[prop], "normal", `closed window has the expected value for ${prop}`);
  }
}

function checkTab(tab, windowId, incognito) {
  for (let prop of ["highlighted", "active", "pinned"]) {
    is(tab[prop], false, `closed tab has the expected value for ${prop}`);
  }
  is(tab.windowId, windowId, "closed tab has the expected value for windowId");
  is(tab.incognito, incognito, "closed tab has the expected value for incognito");
}

function checkRecentlyClosed(recentlyClosed, expectedCount, windowId, incognito = false) {
  let sessionIds = new Set();
  is(recentlyClosed.length, expectedCount, "the expected number of closed tabs/windows was found");
  for (let item of recentlyClosed) {
    if (item.window) {
      sessionIds.add(item.window.sessionId);
      checkWindow(item.window);
    } else if (item.tab) {
      sessionIds.add(item.tab.sessionId);
      checkTab(item.tab, windowId, incognito);
    }
  }
  is(sessionIds.size, expectedCount, "each item has a unique sessionId");
}
