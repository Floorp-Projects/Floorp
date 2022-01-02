/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var TEST_URL =
  "<!DOCTYPE html><style>" +
  "body {margin:0; padding: 0;} " +
  "div {width:100px;height:100px;background:red;} " +
  ".resize-change-color {width:50px;height:50px;background:blue;} " +
  ".change-color {width:50px;height:50px;background:yellow;} " +
  ".add-class {}" +
  "</style><div></div>";
TEST_URL = "data:text/html;charset=utf8," + encodeURIComponent(TEST_URL);

var test = makeTimelineTest("browser_timelineMarkers-frame-05.js", TEST_URL);
