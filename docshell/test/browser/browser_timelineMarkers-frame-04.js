/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

// This file expects frame-head.js to be loaded in the environment.
/* import-globals-from frame-head.js */

"use strict";

// Test that the docShell profile timeline API returns the right
// markers for XMLHttpRequest events.

var TESTS = [
  {
    desc: "Event dispatch from XMLHttpRequest",
    searchFor(markers) {
      return markers.filter(m => m.name == "DOMEvent").length >= 5;
    },
    setup(docShell) {
      content.dispatchEvent(new content.Event("dog"));
    },
    check(markers) {
      let domMarkers = markers.filter(m => m.name == "DOMEvent");
      // One subtlety here is that we have five events: the event we
      // inject in "setup", plus the four state transition events.  The
      // first state transition is reported synchronously and so should
      // show up as a nested marker.
      is(domMarkers.length, 5, "Got 5 markers");

      // We should see some Javascript markers, and they should have a
      // cause.
      let jsMarkers = markers.filter(
        m => m.name == "Javascript" && m.causeName
      );
      ok(!!jsMarkers.length, "Got some Javascript markers");
      is(
        jsMarkers[0].stack.functionDisplayName,
        "do_xhr",
        "Javascript marker has entry point name"
      );
    },
  },
];

if (
  !Services.prefs.getBoolPref(
    "javascript.options.asyncstack_capture_debuggee_only"
  )
) {
  TESTS.push(
    {
      desc: "Async stack trace on Javascript marker",
      searchFor: markers => {
        return markers.some(
          m => m.name == "Javascript" && m.causeName == "promise callback"
        );
      },
      setup(docShell) {
        content.dispatchEvent(new content.Event("promisetest"));
      },
      check(markers) {
        markers = markers.filter(
          m => m.name == "Javascript" && m.causeName == "promise callback"
        );
        ok(!!markers.length, "Found a Javascript marker");

        let frame = markers[0].stack;
        ok(frame.asyncParent !== null, "Parent frame has async parent");
        is(
          frame.asyncParent.asyncCause,
          "promise callback",
          "Async parent has correct cause"
        );
        let asyncFrame = frame.asyncParent;
        // Skip over self-hosted parts of our Promise implementation.
        while (asyncFrame.source === "self-hosted") {
          asyncFrame = asyncFrame.parent;
        }
        is(
          asyncFrame.functionDisplayName,
          "do_promise",
          "Async parent has correct function name"
        );
      },
    },
    {
      desc: "Async stack trace on Javascript marker with script",
      searchFor: markers => {
        return markers.some(
          m => m.name == "Javascript" && m.causeName == "promise callback"
        );
      },
      setup(docShell) {
        content.dispatchEvent(new content.Event("promisescript"));
      },
      check(markers) {
        markers = markers.filter(
          m => m.name == "Javascript" && m.causeName == "promise callback"
        );
        ok(!!markers.length, "Found a Javascript marker");

        let frame = markers[0].stack;
        ok(frame.asyncParent !== null, "Parent frame has async parent");
        is(
          frame.asyncParent.asyncCause,
          "promise callback",
          "Async parent has correct cause"
        );
        let asyncFrame = frame.asyncParent;
        // Skip over self-hosted parts of our Promise implementation.
        while (asyncFrame.source === "self-hosted") {
          asyncFrame = asyncFrame.parent;
        }
        is(
          asyncFrame.functionDisplayName,
          "do_promise_script",
          "Async parent has correct function name"
        );
      },
    }
  );
}

timelineContentTest(TESTS);
