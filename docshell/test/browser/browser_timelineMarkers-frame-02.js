/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell profile timeline API returns the right markers when
// restyles, reflows and paints occur

function rectangleContains(rect, x, y, width, height) {
  return rect.x <= x && rect.y <= y && rect.width >= width &&
    rect.height >= height;
}

let TESTS = [{
  desc: "Changing the width of the test element",
  searchFor: "Paint",
  setup: function(docShell) {
    let div = content.document.querySelector("div");
    div.setAttribute("class", "resize-change-color");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    console.log(markers);
    info(JSON.stringify(markers.filter(m => m.name == "Paint")));
    ok(markers.some(m => m.name == "Reflow"), "markers includes Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    for (let marker of markers.filter(m => m.name == "Paint")) {
      // This change should generate at least one rectangle.
      ok(marker.rectangles.length >= 1, "marker has one rectangle");
      // One of the rectangles should contain the div.
      ok(marker.rectangles.some(r => rectangleContains(r, 0, 0, 100, 100)));
    }
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's background color",
  searchFor: "Paint",
  setup: function(docShell) {
    let div = content.document.querySelector("div");
    div.setAttribute("class", "change-color");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    for (let marker of markers.filter(m => m.name == "Paint")) {
      // This change should generate at least one rectangle.
      ok(marker.rectangles.length >= 1, "marker has one rectangle");
      // One of the rectangles should contain the div.
      ok(marker.rectangles.some(r => rectangleContains(r, 0, 0, 50, 50)));
    }
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's classname",
  searchFor: "Paint",
  setup: function(docShell) {
    let div = content.document.querySelector("div");
    div.setAttribute("class", "change-color add-class");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(!markers.some(m => m.name == "Paint"), "markers doesn't include Paint");
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "sync console.time/timeEnd",
  searchFor: "ConsoleTime",
  setup: function(docShell) {
    content.console.time("FOOBAR");
    content.console.timeEnd("FOOBAR");
    let markers = docShell.popProfileTimelineMarkers();
    is(markers.length, 1, "Got one marker");
    is(markers[0].name, "ConsoleTime", "Got ConsoleTime marker");
    is(markers[0].causeName, "FOOBAR", "Got ConsoleTime FOOBAR detail");
    content.console.time("FOO");
    content.setTimeout(() => {
      content.console.time("BAR");
      content.setTimeout(() => {
        content.console.timeEnd("FOO");
        content.console.timeEnd("BAR");
      }, 100);
    }, 100);
  },
  check: function(markers) {
    is(markers.length, 2, "Got 2 markers");
    is(markers[0].name, "ConsoleTime", "Got first ConsoleTime marker");
    is(markers[0].causeName, "FOO", "Got ConsoleTime FOO detail");
    is(markers[1].name, "ConsoleTime", "Got second ConsoleTime marker");
    is(markers[1].causeName, "BAR", "Got ConsoleTime BAR detail");
  }
}];

timelineContentTest(TESTS);
