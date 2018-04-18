/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { drawText } = require("devtools/client/memory/components/tree-map/draw");

add_task(async function() {
  // Mock out the Canvas2dContext
  let ctx = {
    fillText: (...args) => fillTextValues.push(args),
    measureText: (text) => {
      let width = text ? text.length * 10 : 0;
      return { width };
    }
  };
  let node = {
    x: 20,
    y: 30,
    dx: 500,
    dy: 70,
    name: "Example Node",
    totalBytes: 1200,
    totalCount: 100
  };
  let ratio = 0;
  let borderWidth = () => 1;
  let dragZoom = {
    offsetX: 0,
    offsetY: 0,
    zoom: 0
  };
  let fillTextValues = [];
  let padding = [10, 10];

  drawText(ctx, node, borderWidth, ratio, dragZoom, padding);
  deepEqual(fillTextValues[0], ["Example Node", 11.5, 21.5],
    "Fills in the full node name");
  deepEqual(fillTextValues[1], ["1KiB 100 count", 141.5, 21.5],
    "Includes the full byte and count information");

  fillTextValues = [];
  node.dx = 250;
  drawText(ctx, node, borderWidth, ratio, dragZoom, padding);

  deepEqual(fillTextValues[0], ["Example Node", 11.5, 21.5],
    "Fills in the full node name");
  deepEqual(fillTextValues[1], undefined,
    "Drops off the byte and count information if not enough room");

  fillTextValues = [];
  node.dx = 100;
  drawText(ctx, node, borderWidth, ratio, dragZoom, padding);

  deepEqual(fillTextValues[0], ["Exampl...", 11.5, 21.5],
    "Cuts the name with ellipsis");
  deepEqual(fillTextValues[1], undefined,
    "Drops off the byte and count information if not enough room");

  fillTextValues = [];
  node.dx = 40;
  drawText(ctx, node, borderWidth, ratio, dragZoom, padding);

  deepEqual(fillTextValues[0], ["...", 11.5, 21.5],
    "Shows only ellipsis when smaller");
  deepEqual(fillTextValues[1], undefined,
    "Drops off the byte and count information if not enough room");

  fillTextValues = [];
  node.dx = 20;
  drawText(ctx, node, borderWidth, ratio, dragZoom, padding);

  deepEqual(fillTextValues[0], undefined,
    "Draw nothing when not enough room");
  deepEqual(fillTextValues[1], undefined,
    "Drops off the byte and count information if not enough room");
});
