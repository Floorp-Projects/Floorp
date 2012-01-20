/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  ok(Tilt,
    "The Tilt object wasn't got correctly via defineLazyGetter.");
  is(Tilt.chromeWindow, window,
    "The top-level window wasn't saved correctly");
  ok(Tilt.visualizers,
    "The holder object for all the instances of the visualizer doesn't exist.")
  ok(Tilt.NOTIFICATIONS,
    "The notifications constants weren't referenced correctly.");
}
