/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell profile timeline API returns the right
// markers for XMLHttpRequest events.

let TESTS = [{
  desc: "Event dispatch from XMLHttpRequest",
  searchFor: function(markers) {
    return markers.filter(m => m.name == "DOMEvent").length >= 5;
  },
  setup: function(docShell) {
    content.dispatchEvent(new content.Event("dog"));
  },
  check: function(markers) {
    let domMarkers = markers.filter(m => m.name == "DOMEvent");
    // One subtlety here is that we have five events: the event we
    // inject in "setup", plus the four state transition events.  The
    // first state transition is reported synchronously and so should
    // show up as a nested marker.
    is(domMarkers.length, 5, "Got 5 markers");

    // We should see some Javascript markers, and they should have a
    // cause.
    let jsMarkers = markers.filter(m => m.name == "Javascript" && m.causeName);
    ok(jsMarkers.length > 0, "Got some Javascript markers");
  }
}];

timelineContentTest(TESTS);
