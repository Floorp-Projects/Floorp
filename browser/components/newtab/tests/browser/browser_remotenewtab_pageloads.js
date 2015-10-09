/* globals XPCOMUtils, Task, RemoteAboutNewTab, RemoteNewTabLocation, ok */
"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RemoteNewTabLocation",
  "resource:///modules/RemoteNewTabLocation.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemoteAboutNewTab",
  "resource:///modules/RemoteAboutNewTab.jsm");

const TEST_URL = "https://example.com/browser/browser/components/newtab/tests/browser/dummy_page.html";

let tests = [];

/*
 * Tests that:
 * 1. overriding the RemoteNewTabPageLocation url causes a remote newtab page
 *    to load with the new url.
 * 2. Messages pass between remote page <--> newTab.js <--> RemoteAboutNewTab.js
 */
tests.push(Task.spawn(function* testMessage() {
  yield new Promise(resolve => {
    RemoteAboutNewTab.pageListener.addMessageListener("NewTab:testMessage", () => {
      ok(true, "message received");
      resolve();
    });
  });
}));

add_task(function* open_newtab() {
  RemoteNewTabLocation.override(TEST_URL);
  let tabOptions = {
    gBrowser,
    url: "about:remote-newtab"
  };

  function* testLoader() {
    yield Promise.all(tests);
  }

  yield BrowserTestUtils.withNewTab(
      tabOptions,
      browser => testLoader // jshint ignore:line
  );
});
