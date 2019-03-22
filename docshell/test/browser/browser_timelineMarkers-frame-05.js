/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This file expects frame-head.js to be loaded in the environment.
/* import-globals-from frame-head.js */

"use strict";

function forceSyncReflow(div) {
  div.setAttribute("class", "resize-change-color");
  // Force a reflow.
  return div.offsetWidth;
}

function testSendingEvent() {
  content.document.body.dispatchEvent(new content.Event("dog"));
}

function testConsoleTime() {
  content.console.time("cats");
}

function testConsoleTimeEnd() {
  content.console.timeEnd("cats");
}

function makePromise() {
  let resolver;
  new Promise(function(resolve, reject) {
    testConsoleTime();
    resolver = resolve;
  }).then(function(val) {
    testConsoleTimeEnd();
  });
  return resolver;
}

function resolvePromise(resolver) {
  resolver(23);
}

var TESTS = [{
  desc: "Stack trace on sync reflow",
  searchFor: "Reflow",
  setup(docShell) {
    let div = content.document.querySelector("div");
    forceSyncReflow(div);
  },
  check(markers) {
    markers = markers.filter(m => m.name == "Reflow");
    ok(markers.length > 0, "Reflow marker includes stack");
    ok(markers[0].stack.functionDisplayName == "forceSyncReflow");
  },
}, {
  desc: "Stack trace on DOM event",
  searchFor: "DOMEvent",
  setup(docShell) {
    content.document.body.addEventListener("dog",
                                           function(e) { console.log("hi"); },
                                           true);
    testSendingEvent();
  },
  check(markers) {
    markers = markers.filter(m => m.name == "DOMEvent");
    ok(markers.length > 0, "DOMEvent marker includes stack");
    ok(markers[0].stack.functionDisplayName == "testSendingEvent",
       "testSendingEvent is on the stack");
  },
}, {
  desc: "Stack trace on console event",
  searchFor: "ConsoleTime",
  setup(docShell) {
    testConsoleTime();
    testConsoleTimeEnd();
  },
  check(markers) {
    markers = markers.filter(m => m.name == "ConsoleTime");
    ok(markers.length > 0, "ConsoleTime marker includes stack");
    ok(markers[0].stack.functionDisplayName == "testConsoleTime",
       "testConsoleTime is on the stack");
    ok(markers[0].endStack.functionDisplayName == "testConsoleTimeEnd",
       "testConsoleTimeEnd is on the stack");
  },
}];

if (Services.prefs.getBoolPref("javascript.options.asyncstack")) {
  TESTS.push({
    desc: "Async stack trace on Promise",
    searchFor: "ConsoleTime",
    setup(docShell) {
      let resolver = makePromise();
      resolvePromise(resolver);
    },
    check(markers) {
      markers = markers.filter(m => m.name == "ConsoleTime");
      ok(markers.length > 0, "Promise marker includes stack");
      ok(markers[0].stack.functionDisplayName == "testConsoleTime",
         "testConsoleTime is on the stack");
      let frame = markers[0].endStack;
      ok(frame.functionDisplayName == "testConsoleTimeEnd",
         "testConsoleTimeEnd is on the stack");

      frame = frame.parent;
      ok(frame.functionDisplayName == "makePromise/<",
         "makePromise/< is on the stack");
      let asyncFrame = frame.asyncParent;
      ok(asyncFrame !== null, "Frame has async parent");
      is(asyncFrame.asyncCause, "promise callback",
         "Async parent has correct cause");
      // Skip over self-hosted parts of our Promise implementation.
      while (asyncFrame.source === "self-hosted") {
        asyncFrame = asyncFrame.parent;
      }
      is(asyncFrame.functionDisplayName, "makePromise",
         "Async parent has correct function name");
    },
  });
}

timelineContentTest(TESTS);
