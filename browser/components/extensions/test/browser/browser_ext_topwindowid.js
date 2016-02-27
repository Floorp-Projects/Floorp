/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_topwindowid_cleanup() {
  let {Frames} = Cu.import("resource://gre/modules/ExtensionManagement.jsm", {});

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let {outerWindowID, messageManager} = tab.linkedBrowser;

  ok(Frames.topWindowIds.has(outerWindowID), "Outer window ID is registered");

  let awaitDisconnect = TestUtils.topicObserved("message-manager-disconnect",
                                                subject => subject === messageManager);

  yield BrowserTestUtils.removeTab(tab);

  yield awaitDisconnect;

  ok(!Frames.topWindowIds.has(outerWindowID), "Outer window ID is no longer registered");
});

