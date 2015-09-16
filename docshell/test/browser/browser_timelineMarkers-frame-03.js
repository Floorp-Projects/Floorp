/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell profile timeline API returns the right
// markers for DOM events.

var TESTS = [{
  desc: "Event dispatch with single handler",
  searchFor: 'DOMEvent',
  setup: function(docShell) {
    content.document.body.addEventListener("dog",
                                           function(e) { console.log("hi"); },
                                           true);
    content.document.body.dispatchEvent(new content.Event("dog"));
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == 'DOMEvent');
    is(markers.length, 1, "Got 1 marker");
    is(markers[0].type, "dog", "Got dog event name");
    is(markers[0].eventPhase, 2, "Got phase 2");
  }
}, {
  desc: "Event dispatch with a second handler",
  searchFor: function(markers) {
    return markers.filter(m => m.name == 'DOMEvent').length >= 2;
  },
  setup: function(docShell) {
    content.document.body.addEventListener("dog",
                                           function(e) { console.log("hi"); },
                                           false);
    content.document.body.dispatchEvent(new content.Event("dog"));
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == 'DOMEvent');
    is(markers.length, 2, "Got 2 markers");
  }
}, {
  desc: "Event targeted at child",
  searchFor: function(markers) {
    return markers.filter(m => m.name == 'DOMEvent').length >= 2;
  },
  setup: function(docShell) {
    let child = content.document.body.firstElementChild;
    child.addEventListener("dog", function(e) { });
    child.dispatchEvent(new content.Event("dog"));
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == 'DOMEvent');
    is(markers.length, 2, "Got 2 markers");
    is(markers[0].eventPhase, 1, "Got phase 1 marker");
    is(markers[1].eventPhase, 2, "Got phase 2 marker");
  }
}, {
  desc: "Event dispatch on a new document",
  searchFor: function(markers) {
    return markers.filter(m => m.name == 'DOMEvent').length >= 2;
  },
  setup: function(docShell) {
    let doc = content.document.implementation.createHTMLDocument("doc");
    let p = doc.createElement("p");
    p.innerHTML = "inside";
    doc.body.appendChild(p);

    p.addEventListener("zebra", function(e) {console.log("hi");});
    p.dispatchEvent(new content.Event("zebra"));
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == 'DOMEvent');
    is(markers.length, 1, "Got 1 marker");
  }
}, {
  desc: "Event dispatch on window",
  searchFor: function(markers) {
    return markers.filter(m => m.name == 'DOMEvent').length >= 2;
  },
  setup: function(docShell) {
    let doc = content.window.addEventListener("aardvark", function(e) {
      console.log("I like ants!");
    });

    content.window.dispatchEvent(new content.Event("aardvark"));
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == 'DOMEvent');
    is(markers.length, 1, "Got 1 marker");
  }
}];

timelineContentTest(TESTS);
