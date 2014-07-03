/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the CubicBezierWidget events

const TEST_URI = "chrome://browser/content/devtools/cubic-bezier-frame.xhtml";
const {CubicBezierWidget, PREDEFINED} =
  devtools.require("devtools/shared/widgets/CubicBezierWidget");

let test = Task.async(function*() {
  yield promiseTab(TEST_URI);

  let container = content.document.querySelector("#container");
  let w = new CubicBezierWidget(container, PREDEFINED.linear);

  yield pointsCanBeDragged(w);
  yield curveCanBeClicked(w);
  yield pointsCanBeMovedWithKeyboard(w);

  w.destroy();
  gBrowser.removeCurrentTab();
  finish();
});

function* pointsCanBeDragged(widget) {
  info("Checking that the control points can be dragged with the mouse");

  info("Listening for the update event");
  let onUpdated = widget.once("updated");

  info("Generating a mousedown/move/up on P1");
  widget._onPointMouseDown({target: widget.p1});
  EventUtils.synthesizeMouse(content.document.documentElement, 0, 100,
    {type: "mousemove"}, content.window);
  EventUtils.synthesizeMouse(content.document.documentElement, 0, 100,
    {type: "mouseup"}, content.window);

  let bezier = yield onUpdated;
  ok(true, "The widget fired the updated event");
  ok(bezier, "The updated event contains a bezier argument");
  is(bezier.P1[0], 0, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 1, "The new P1 progress coordinate is correct");

  info("Listening for the update event");
  let onUpdated = widget.once("updated");

  info("Generating a mousedown/move/up on P2");
  widget._onPointMouseDown({target: widget.p2});
  EventUtils.synthesizeMouse(content.document.documentElement, 200, 300,
    {type: "mousemove"}, content.window);
  EventUtils.synthesizeMouse(content.document.documentElement, 200, 300,
    {type: "mouseup"}, content.window);

  let bezier = yield onUpdated;
  is(bezier.P2[0], 1, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 0, "The new P2 progress coordinate is correct");
}

function* curveCanBeClicked(widget) {
  info("Checking that clicking on the curve moves the closest control point");

  info("Listening for the update event");
  let onUpdated = widget.once("updated");

  info("Click close to P1");
  widget._onCurveClick({pageX: 50, pageY: 150});

  let bezier = yield onUpdated;
  ok(true, "The widget fired the updated event");
  is(bezier.P1[0], 0.25, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.75, "The new P1 progress coordinate is correct");
  is(bezier.P2[0], 1, "P2 time coordinate remained unchanged");
  is(bezier.P2[1], 0, "P2 progress coordinate remained unchanged");

  info("Listening for the update event");
  let onUpdated = widget.once("updated");

  info("Click close to P2");
  widget._onCurveClick({pageX: 150, pageY: 250});

  let bezier = yield onUpdated;
  is(bezier.P2[0], 0.75, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 0.25, "The new P2 progress coordinate is correct");
  is(bezier.P1[0], 0.25, "P1 time coordinate remained unchanged");
  is(bezier.P1[1], 0.75, "P1 progress coordinate remained unchanged");
}

function* pointsCanBeMovedWithKeyboard(widget) {
  info("Checking that points respond to keyboard events");

  info("Moving P1 to the left");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 37));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.235, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.75, "The new P1 progress coordinate is correct");

  info("Moving P1 to the left, fast");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 37, true));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.085, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.75, "The new P1 progress coordinate is correct");

  info("Moving P1 to the right, fast");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 39, true));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.235, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.75, "The new P1 progress coordinate is correct");

  info("Moving P1 to the bottom");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 40));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.235, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.735, "The new P1 progress coordinate is correct");

  info("Moving P1 to the bottom, fast");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 40, true));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.235, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.585, "The new P1 progress coordinate is correct");

  info("Moving P1 to the top, fast");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p1, 38, true));
  let bezier = yield onUpdated;
  is(bezier.P1[0], 0.235, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0.735, "The new P1 progress coordinate is correct");

  info("Checking that keyboard events also work with P2");
  info("Moving P2 to the left");
  let onUpdated = widget.once("updated");
  widget._onPointKeyDown(getKeyEvent(widget.p2, 37));
  let bezier = yield onUpdated;
  is(bezier.P2[0], 0.735, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 0.25, "The new P2 progress coordinate is correct");
}

function getKeyEvent(target, keyCode, shift=false) {
  return {
    target: target,
    keyCode: keyCode,
    shiftKey: shift,
    preventDefault: () => {}
  };
}
