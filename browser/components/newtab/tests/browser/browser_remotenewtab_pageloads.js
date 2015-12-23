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
const NEWTAB_URL = "about:remote-newtab";

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
  ok(RemoteNewTabLocation.href === TEST_URL, "RemoteNewTabLocation has been overridden");
  let tabOptions = {
    gBrowser,
    url: NEWTAB_URL,
  };

  for (let test of tests) {
    yield BrowserTestUtils.withNewTab(tabOptions, function* (browser) { // jshint ignore:line
      yield test;
    }); // jshint ignore:line
  }
});
