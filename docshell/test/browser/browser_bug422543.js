/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function SHistoryListener() {
}

SHistoryListener.prototype = {
  retval: true,
  last: "initial",

  OnHistoryNewEntry: function (aNewURI) {
    this.last = "newentry";
  },

  OnHistoryGoBack: function (aBackURI) {
    this.last = "goback";
    return this.retval;
  },

  OnHistoryGoForward: function (aForwardURI) {
    this.last = "goforward";
    return this.retval;
  },

  OnHistoryGotoIndex: function (aIndex, aGotoURI) {
    this.last = "gotoindex";
    return this.retval;
  },

  OnHistoryPurge: function (aNumEntries) {
    this.last = "purge";
    return this.retval;
  },

  OnHistoryReload: function (aReloadURI, aReloadFlags) {
    this.last = "reload";
    return this.retval;
  },

  OnHistoryReplaceEntry: function (aIndex) {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISHistoryListener,
                                         Ci.nsISupportsWeakReference])
};

let gFirstListener = new SHistoryListener();
let gSecondListener = new SHistoryListener();

function test() {
  TestRunner.run();
}

function runTests() {
  yield setup();
  let browser = gBrowser.selectedBrowser;
  checkListeners("initial", "listeners initialized");

  // Check if all history listeners are always notified.
  info("# part 1");
  browser.loadURI("http://www.example.com/");
  yield whenPageShown(browser);
  checkListeners("newentry", "shistory has a new entry");
  ok(browser.canGoBack, "we can go back");

  browser.goBack();
  yield whenPageShown(browser);
  checkListeners("goback", "back to the first shentry");
  ok(browser.canGoForward, "we can go forward");

  browser.goForward();
  yield whenPageShown(browser);
  checkListeners("goforward", "forward to the second shentry");

  browser.reload();
  yield whenPageShown(browser);
  checkListeners("reload", "current shentry reloaded");

  browser.gotoIndex(0);
  yield whenPageShown(browser);
  checkListeners("gotoindex", "back to the first index");

  // Check nsISHistoryInternal.notifyOnHistoryReload
  info("# part 2");
  ok(notifyReload(), "reloading has not been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let the first listener cancel the reload action.
  info("# part 3");
  resetListeners();
  gFirstListener.retval = false;
  ok(!notifyReload(), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let both listeners cancel the reload action.
  info("# part 4");
  resetListeners();
  gSecondListener.retval = false;
  ok(!notifyReload(), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");

  // Let the second listener cancel the reload action.
  info("# part 5");
  resetListeners();
  gFirstListener.retval = true;
  ok(!notifyReload(), "reloading has been canceled");
  checkListeners("reload", "saw the reload notification");
}

function checkListeners(aLast, aMessage) {
  is(gFirstListener.last, aLast, aMessage);
  is(gSecondListener.last, aLast, aMessage);
}

function resetListeners() {
  gFirstListener.last = gSecondListener.last = "initial";
}

function notifyReload() {
  let browser = gBrowser.selectedBrowser;
  let shistory = browser.docShell.sessionHistory;
  shistory.QueryInterface(Ci.nsISHistoryInternal);
  return shistory.notifyOnHistoryReload(browser.currentURI, 0);
}

function setup(aCallback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  registerCleanupFunction(function () { gBrowser.removeTab(tab); });

  whenPageShown(browser, function () {
    gFirstListener = new SHistoryListener();
    gSecondListener = new SHistoryListener();

    let shistory = browser.docShell.sessionHistory;
    shistory.addSHistoryListener(gFirstListener);
    shistory.addSHistoryListener(gSecondListener);

    registerCleanupFunction(function () {
      shistory.removeSHistoryListener(gFirstListener);
      shistory.removeSHistoryListener(gSecondListener);
    });

    (aCallback || TestRunner.next)();
  });
}

function whenPageShown(aBrowser, aCallback) {
  aBrowser.addEventListener("pageshow", function onLoad() {
    aBrowser.removeEventListener("pageshow", onLoad, true);
    executeSoon(aCallback || TestRunner.next);
  }, true);
}

let TestRunner = {
  run: function () {
    waitForExplicitFinish();
    this._iter = runTests();
    this.next();
  },

  next: function () {
    try {
      TestRunner._iter.next();
    } catch (e if e instanceof StopIteration) {
      TestRunner.finish();
    }
  },

  finish: function () {
    finish();
  }
};
