/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    ok(!ed.hasMarker(0, "breakpoints", "test"), "default is ok");
    ed.addMarker(0, "breakpoints", "test");
    ed.addMarker(0, "breakpoints", "test2");
    ok(ed.hasMarker(0, "breakpoints", "test"), "addMarker/1");
    ok(ed.hasMarker(0, "breakpoints", "test2"), "addMarker/2");

    ed.removeMarker(0, "breakpoints", "test");
    ok(!ed.hasMarker(0, "breakpoints", "test"), "removeMarker/1");
    ok(ed.hasMarker(0, "breakpoints", "test2"), "removeMarker/2");

    ed.removeAllMarkers("breakpoints");
    ok(!ed.hasMarker(0, "breakpoints", "test"), "removeAllMarkers/1");
    ok(!ed.hasMarker(0, "breakpoints", "test2"), "removeAllMarkers/2");

    ed.addMarker(0, "breakpoints", "breakpoint");
    ed.setMarkerListeners(0, "breakpoints", "breakpoint", {
      "click": (line, marker, param) => {
        is(line, 0, "line is ok");
        is(marker.className, "breakpoint", "marker is ok");
        ok(param, "click is ok");

        teardown(ed, win);
      }
    }, [ true ]);

    const env = win.document.querySelector("iframe").contentWindow;
    const div = env.document.querySelector("div.breakpoint");
    div.click();
  });
}
