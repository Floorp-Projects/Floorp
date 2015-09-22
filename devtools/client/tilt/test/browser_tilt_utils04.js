/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let dom = TiltUtils.DOM;
  ok(dom, "The TiltUtils.DOM wasn't found.");


  is(dom.initCanvas(), null,
    "The initCanvas() function shouldn't work if no parent node is set.");


  dom.parentNode = gBrowser.parentNode;
  ok(dom.parentNode,
    "The necessary parent node wasn't found.");


  let canvas = dom.initCanvas(null, {
    append: true,
    focusable: true,
    width: 123,
    height: 456,
    id: "tilt-test-canvas"
  });

  is(canvas.width, 123,
    "The test canvas doesn't have the correct width set.");
  is(canvas.height, 456,
    "The test canvas doesn't have the correct height set.");
  is(canvas.getAttribute("tabindex"), 1,
    "The test canvas tab index wasn't set correctly.");
  is(canvas.getAttribute("id"), "tilt-test-canvas",
    "The test canvas doesn't have the correct id set.");
  ok(dom.parentNode.ownerDocument.getElementById(canvas.id),
    "A canvas should be appended to the parent node if specified.");
  canvas.parentNode.removeChild(canvas);

  let canvas2 = dom.initCanvas(null, { id: "tilt-test-canvas2" });

  is(canvas2.width, dom.parentNode.clientWidth,
    "The second test canvas doesn't have the implicit width set.");
  is(canvas2.height, dom.parentNode.clientHeight,
    "The second test canvas doesn't have the implicit height set.");
  is(canvas2.id, "tilt-test-canvas2",
    "The second test canvas doesn't have the correct id set.");
  is(dom.parentNode.ownerDocument.getElementById(canvas2.id), null,
    "A canvas shouldn't be appended to the parent node if not specified.");


  dom.parentNode = null;
  is(dom.parentNode, null,
    "The necessary parent node shouldn't be found anymore.");
}
