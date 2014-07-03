/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the CubicBezierWidget generates content in a given parent node

const TEST_URI = "chrome://browser/content/devtools/cubic-bezier-frame.xhtml";
const {CubicBezierWidget} = devtools.require("devtools/shared/widgets/CubicBezierWidget");

let test = Task.async(function*() {
  yield promiseTab(TEST_URI);

  info("Checking that the markup is created in the parent");
  let container = content.document.querySelector("#container");
  let w = new CubicBezierWidget(container);

  ok(container.querySelector(".coordinate-plane"),
    "The coordinate plane has been added");
  let buttons = container.querySelectorAll("button");
  is(buttons.length, 2,
    "The 2 control points have been added");
  is(buttons[0].className, "control-point");
  is(buttons[0].id, "P1");
  is(buttons[1].className, "control-point");
  is(buttons[1].id, "P2");
  ok(container.querySelector("canvas"), "The curve canvas has been added");

  info("Destroying the widget");
  w.destroy();
  is(container.children.length, 0, "All nodes have been removed");

  gBrowser.removeCurrentTab();
  finish();
});
