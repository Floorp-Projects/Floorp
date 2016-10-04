/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell has the right profile timeline API

const URL = "data:text/html;charset=utf-8,Test page";

add_task(function* () {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: URL },
    function* (browser) {
      yield ContentTask.spawn(browser, null, function() {
        ok("recordProfileTimelineMarkers" in docShell,
           "The recordProfileTimelineMarkers attribute exists");
        ok("popProfileTimelineMarkers" in docShell,
           "The popProfileTimelineMarkers function exists");
        ok(docShell.recordProfileTimelineMarkers === false,
           "recordProfileTimelineMarkers is false by default");
        ok(docShell.popProfileTimelineMarkers().length === 0,
           "There are no markers by default");

        docShell.recordProfileTimelineMarkers = true;
        ok(docShell.recordProfileTimelineMarkers === true,
           "recordProfileTimelineMarkers can be set to true");

        docShell.recordProfileTimelineMarkers = false;
        ok(docShell.recordProfileTimelineMarkers === false,
           "recordProfileTimelineMarkers can be set to false");
      });
    });
});
