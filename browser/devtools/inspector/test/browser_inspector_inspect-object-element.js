/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// A regression test for bug 665880 to make sure elements inside <object> can
// be inspected without exceptions.

const TEST_URI = "data:text/html;charset=utf-8," +
  "<object><p>browser_inspector_inspect-object-element.js</p></object>";

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URI);
  let objectNode = getNode("object");
  ok(objectNode, "We have the object node");

  yield selectNode(objectNode, inspector);
});
