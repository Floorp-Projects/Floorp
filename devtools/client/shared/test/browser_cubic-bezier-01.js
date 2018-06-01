/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the CubicBezierWidget generates content in a given parent node

const {CubicBezierWidget} =
  require("devtools/client/shared/widgets/CubicBezierWidget");

const TEST_URI = CHROME_URL_ROOT + "doc_cubic-bezier-01.html";

add_task(async function() {
  const [host,, doc] = await createHost("bottom", TEST_URI);

  info("Checking that the graph markup is created in the parent");
  const container = doc.querySelector("#cubic-bezier-container");
  const w = new CubicBezierWidget(container);

  ok(container.querySelector(".display-wrap"),
    "The display has been added");

  ok(container.querySelector(".coordinate-plane"),
    "The coordinate plane has been added");
  const buttons = container.querySelectorAll("button");
  is(buttons.length, 2,
    "The 2 control points have been added");
  is(buttons[0].className, "control-point");
  is(buttons[1].className, "control-point");
  ok(container.querySelector("canvas"), "The curve canvas has been added");

  info("Destroying the widget");
  w.destroy();
  is(container.children.length, 0, "All nodes have been removed");

  host.destroy();
});
