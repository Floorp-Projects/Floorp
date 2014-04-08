/* globals Components: true, Promise: true, gBrowser: true, Test: true,
           info: true, is: true, window: true, waitForExplicitFinish: true,
           finish: true, ok: true*/

"use strict";

const { interfaces: Ci, classes: Cc, utils: Cu } = Components;
const { addObserver, removeObserver } = Cc["@mozilla.org/observer-service;1"].
                                          getService(Ci.nsIObserverService);
const { openWindow } = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                         getService(Ci.nsIWindowWatcher);

const Test = routine => () => {
  waitForExplicitFinish();
  Task.spawn(routine)
      .then(finish, error => {
        ok(false, error);
        finish();
      });
};

// Returns promise for the observer notification subject for
// the given topic. If `receive("foo")` is called `n` times
// nth promise is resolved on an `nth` "foo" notification.
const receive = (topic, p, syncCallback) => {
  const { promise, resolve, reject } = Promise.defer();
  const { queue } = receive;
  const timeout = () => {
    queue.splice(queue.indexOf(resolve) - 1, 2);
    reject(new Error("Timeout"));
  };

  const observer = {
    observe: subject => {
      // Browser loads bunch of other documents that we don't care
      // about so we let allow filtering notifications via `p` function.
      if (p && !p(subject)) return;
      // If observer is a first one with a given `topic`
      // in a queue resolve promise and take it off the queue
      // otherwise keep waiting.
      const index = queue.indexOf(topic);
      if (queue.indexOf(resolve) === index + 1) {
        removeObserver(observer, topic);
        clearTimeout(id, reject);
        queue.splice(index, 2);
        // Some tests need to be executed synchronously when the event is fired.
        if (syncCallback) {
          syncCallback(subject);
        }
        resolve(subject);
      }
    }
  };
  const id = setTimeout(timeout, 90000);
  addObserver(observer, topic, false);
  queue.push(topic, resolve);

  return promise;
};
receive.queue = [];

const openTab = uri => gBrowser.selectedTab = gBrowser.addTab(uri);

const sleep = ms => {
  const { promise, resolve } = Promise.defer();
  setTimeout(resolve, ms);
  return promise;
};

const isData = document => document.URL.startsWith("data:");

const uri1 = "data:text/html;charset=utf-8,<h1>1</h1>";
// For whatever reason going back on load event doesn't work so timeout it is :(
const uri2 = "data:text/html;charset=utf-8,<h1>2</h1><script>setTimeout(SpecialPowers.wrap(window).back,100)</script>";
const uri3 = "data:text/html;charset=utf-8,<h1>3</h1>";

const uri4 = "chrome://browser/content/license.html";

const test = Test(function*() {
  let documentInteractive = receive("content-document-interactive", isData, d => {
    // This test is executed synchronously when the event is received.
    is(d.readyState, "interactive", "document is interactive");
    is(d.URL, uri1, "document.URL matches tab url");
  });
  let documentLoaded = receive("content-document-loaded", isData);
  let pageShown = receive("content-page-shown", isData);

  info("open: uri#1");
  const tab1 = openTab(uri1);
  const browser1 = gBrowser.getBrowserForTab(tab1);

  let interactiveDocument1 = yield documentInteractive;

  let loadedDocument1 = yield documentLoaded;
  is(loadedDocument1.readyState, "complete", "document is loaded");
  is(interactiveDocument1, loadedDocument1, "interactive document is loaded");

  let shownPage = yield pageShown;
  is(interactiveDocument1, shownPage, "loaded document is shown");

  // Wait until history entry is created before loading new uri.
  yield receive("sessionstore-state-write-complete");

  info("load uri#2");

  documentInteractive = receive("content-document-interactive", isData, d => {
    // This test is executed synchronously when the event is received.
    is(d.readyState, "interactive", "document is interactive");
    is(d.URL, uri2, "document.URL matches URL loaded");
  });
  documentLoaded = receive("content-document-loaded", isData);
  pageShown = receive("content-page-shown", isData);
  let pageHidden = receive("content-page-hidden", isData);

  browser1.loadURI(uri2);

  let hiddenPage = yield pageHidden;
  is(interactiveDocument1, hiddenPage, "loaded document is hidden");

  let interactiveDocument2 = yield documentInteractive;

  let loadedDocument2 = yield documentLoaded;
  is(loadedDocument2.readyState, "complete", "document is loaded");
  is(interactiveDocument2, loadedDocument2, "interactive document is loaded");

  shownPage = yield pageShown;
  is(interactiveDocument2, shownPage, "loaded document is shown");

  info("go back to uri#1");


  documentInteractive = receive("content-document-interactive", isData, d => {
    // This test is executed synchronously when the event is received.
    is(d.readyState, "interactive", "document is interactive");
    is(d.URL, uri3, "document.URL matches URL loaded");
  });
  documentLoaded = receive("content-document-loaded", isData);
  pageShown = receive("content-page-shown", isData);
  pageHidden = receive("content-page-hidden", isData);

  hiddenPage = yield pageHidden;
  is(interactiveDocument2, hiddenPage, "new document is hidden");

  shownPage = yield pageShown;
  is(interactiveDocument1, shownPage, "previous document is shown");

  info("load uri#3");

  browser1.loadURI(uri3);

  pageShown = receive("content-page-shown", isData);

  let interactiveDocument3 = yield documentInteractive;

  let loadedDocument3 = yield documentLoaded;
  is(loadedDocument3.readyState, "complete", "document is loaded");
  is(interactiveDocument3, loadedDocument3, "interactive document is loaded");

  shownPage = yield pageShown;
  is(interactiveDocument3, shownPage, "previous document is shown");

  gBrowser.removeTab(tab1);

  info("load chrome uri");

  const tab2 = openTab(uri4);
  documentInteractive = receive("chrome-document-interactive", null, d => {
    // This test is executed synchronously when the event is received.
    is(d.readyState, "interactive", "document is interactive");
    is(d.URL, uri4, "document.URL matches URL loaded");
  });
  documentLoaded = receive("chrome-document-loaded");
  pageShown = receive("chrome-page-shown");

  const interactiveDocument4 = yield documentInteractive;

  let loadedDocument4 = yield documentLoaded;
  is(loadedDocument4.readyState, "complete", "document is loaded");
  is(interactiveDocument4, loadedDocument4, "interactive document is loaded");

  shownPage = yield pageShown;
  is(interactiveDocument4, shownPage, "loaded chrome document is shown");

  pageHidden = receive("chrome-page-hidden");
  gBrowser.removeTab(tab2);

  hiddenPage = yield pageHidden;
  is(interactiveDocument4, hiddenPage, "chrome document hidden");
});
