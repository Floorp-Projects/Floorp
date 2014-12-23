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
    markers = markers.filter(m => m.name == "DOMEvent");
    // One subtlety here is that we have five events: the event we
    // inject in "setup", plus the four state transition events.  The
    // first state transition is reported synchronously and so should
    // show up as a nested marker.
    is(markers.length, 5, "Got 5 markers");
  }
}];

timelineContentTest(TESTS);
