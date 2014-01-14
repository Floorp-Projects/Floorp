/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


let tmp = {};
Cu.import("resource:///modules/sessionstore/SessionFile.jsm", tmp);
Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", tmp);
let {SessionFile, TabStateCache} = tmp;

// Shortcuts for histogram names
let Keys = {};
for (let k of ["HISTORY", "FORMDATA", "OPEN_WINDOWS", "CLOSED_WINDOWS", "CLOSED_TABS_IN_OPEN_WINDOWS", "DOM_STORAGE"]) {
  Keys[k] = "FX_SESSION_RESTORE_TOTAL_" + k + "_SIZE_BYTES";
}

function lt(a, b, message) {
  isnot(a, undefined, message + " (sanity check)");
  isnot(b, undefined, message + " (sanity check)");
  ok(a < b, message + " ( " + a + " < " + b + ")");
}
function gt(a, b, message) {
  isnot(a, undefined, message + " (sanity check)");
  isnot(b, undefined, message + " (sanity check)");
  ok(a > b, message + " ( " + a + " > " + b + ")");
}

add_task(function init() {
  for (let i = ss.getClosedWindowCount() - 1; i >= 0; --i) {
    ss.forgetClosedWindow(i);
  }
  for (let i = ss.getClosedTabCount(window) - 1; i >= 0; --i) {
    ss.forgetClosedTab(window, i);
  }
});

/**
 * Test that Telemetry collection doesn't cause any error.
 */
add_task(function() {
  info("Checking a little bit of consistency");
  let statistics = yield promiseStats();

  for (let k of Object.keys(statistics)) {
    let data = statistics[k];
    info("Data for " + k + ": " + data);
    if (Array.isArray(data)) {
      ok(data.every(x => x >= 0), "Data for " + k + " is >= 0");
    } else {
      ok(data >= 0, "Data for " + k + " is >= 0");
    }
  }
});

/**
 * Test HISTORY key.
 */
add_task(function history() {
  let KEY = Keys.HISTORY;
  let tab = gBrowser.addTab("http://example.org:80/?");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  try {
    SyncHandlers.get(tab.linkedBrowser).flush();
    let statistics = yield promiseStats();

    info("Now changing history");
    tab.linkedBrowser.contentWindow.history.pushState({foo:1}, "ref");
    SyncHandlers.get(tab.linkedBrowser).flush();
    let statistics2 = yield promiseStats();

    // We have changed history, so it must have increased
    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of HISTORY has increased");

// Almost nothing else should
    for (let k of ["FORMDATA", "DOM_STORAGE", "CLOSED_WINDOWS", "CLOSED_TABS_IN_OPEN_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }
  } finally {
    if (tab) {
      gBrowser.removeTab(tab);
    }
  }
});

/**
 * Test CLOSED_TABS_IN_OPEN_WINDOWS key.
 */
add_task(function close_tab() {
  let KEY = Keys.CLOSED_TABS_IN_OPEN_WINDOWS;
  let tab = gBrowser.addTab("http://example.org:80/?close_tab");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  try {
    SyncHandlers.get(tab.linkedBrowser).flush();
    let statistics = yield promiseStats();

    info("Now closing a tab");
    gBrowser.removeTab(tab);
    tab = null;
    let statistics2 = yield promiseStats();

    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of CLOSED_TABS_IN_OPEN_WINDOWS has increased");

    // Almost nothing else should change
    for (let k of ["FORMDATA", "DOM_STORAGE", "CLOSED_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }

  } finally {
    if (tab) {
      gBrowser.removeTab(tab);
    }
  }
});

/**
 * Test OPEN_WINDOWS key.
 */
add_task(function open_window() {
  let KEY = Keys.OPEN_WINDOWS;
  let win;
  try {
    let statistics = yield promiseStats();
    win = yield promiseNewWindowLoaded("http://example.org:80/?open_window");
    let statistics2 = yield promiseStats();

    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of OPEN_WINDOWS has increased");

    // Almost nothing else should change
    for (let k of ["FORMDATA", "DOM_STORAGE", "CLOSED_WINDOWS", "CLOSED_TABS_IN_OPEN_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }

  } finally {
    if (win) {
      yield promiseWindowClosed(win);
    }
  }
});

/**
 * Test CLOSED_WINDOWS key.
 */
add_task(function close_window() {
  let KEY = Keys.CLOSED_WINDOWS;
  let win = yield promiseNewWindowLoaded("http://example.org:80/?close_window");

  // We need to add something to the window, otherwise it won't be saved
  let tab = win.gBrowser.addTab("http://example.org:80/?close_tab");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  try {
    let statistics = yield promiseStats();
    yield promiseWindowClosed(win);
    win = null;
    let statistics2 = yield promiseStats();

    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of CLOSED_WINDOWS has increased");
    lt(statistics2[Keys.OPEN_WINDOWS], statistics[Keys.OPEN_WINDOWS], "The total size of OPEN_WINDOWS has decreased");

    // Almost nothing else should change
    for (let k of ["FORMDATA", "DOM_STORAGE", "CLOSED_TABS_IN_OPEN_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }

  } finally {
    if (win) {
      yield promiseWindowClosed(win);
    }
  }
});


/**
 * Test DOM_STORAGE key.
 */
add_task(function dom_storage() {
  let KEY = Keys.DOM_STORAGE;
  let tab = gBrowser.addTab("http://example.org:80/?dom_storage");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  try {
    SyncHandlers.get(tab.linkedBrowser).flush();
    let statistics = yield promiseStats();

    info("Now adding some storage");
    yield modifySessionStorage(tab.linkedBrowser, {foo: "bar"});
    SyncHandlers.get(tab.linkedBrowser).flush();

    let statistics2 = yield promiseStats();

    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of DOM_STORAGE has increased");

    // Almost nothing else should change
    for (let k of ["CLOSED_TABS_IN_OPEN_WINDOWS", "FORMDATA", "CLOSED_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }

  } finally {
    if (tab) {
      gBrowser.removeTab(tab);
    }
  }
});

/**
 * Test FORMDATA key.
 */
add_task(function formdata() {
  let KEY = Keys.FORMDATA;
  let tab = gBrowser.addTab("data:text/html;charset=utf-8,<input%20id='input'>");
  yield promiseBrowserLoaded(tab.linkedBrowser);
  try {
    SyncHandlers.get(tab.linkedBrowser).flush();
    let statistics = yield promiseStats();

    info("Now changing form data");

    yield setInputValue(tab.linkedBrowser, {id: "input", value: "This is some form data"});
    SyncHandlers.get(tab.linkedBrowser).flush();
    TabStateCache.delete(tab.linkedBrowser);

    let statistics2 = yield promiseStats();

    isnot(statistics[KEY], undefined, "Key was defined");
    isnot(statistics2[KEY], undefined, "Key is still defined");
    gt(statistics2[KEY], statistics[KEY], "The total size of FORMDATA has increased");

    // Almost nothing else should
    for (let k of ["DOM_STORAGE", "CLOSED_WINDOWS", "CLOSED_TABS_IN_OPEN_WINDOWS"]) {
      is(statistics2[Keys[k]], statistics[Keys[k]], "The total size of " + k + " has not increased");
    }
  } finally {
    if (tab) {
      gBrowser.removeTab(tab);
    }
  }
});


/**
 * Get the latest statistics.
 */
function promiseStats() {
  let state = ss.getBrowserState();
  info("Stats: " + state);
  return SessionFile.gatherTelemetry(state);
}


function modifySessionStorage(browser, data) {
  browser.messageManager.sendAsyncMessage("ss-test:modifySessionStorage", data);
  return promiseContentMessage(browser, "ss-test:MozStorageChanged");
}

function setInputValue(browser, data) {
  return sendMessage(browser, "ss-test:setInputValue", data);
}
