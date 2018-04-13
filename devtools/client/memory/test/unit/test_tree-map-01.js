/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { drawBox } = require("devtools/client/memory/components/tree-map/draw");

add_task(async function() {
  let fillRectValues, strokeRectValues;
  let ctx = {
    fillRect: (...args) => {
      fillRectValues = args;
    },
    strokeRect: (...args) => {
      strokeRectValues = args;
    }
  };
  let node = {
    x: 20,
    y: 30,
    dx: 50,
    dy: 70,
    type: "other",
    depth: 2
  };
  let padding = [10, 10];
  let borderWidth = () => 1;
  let dragZoom = {
    offsetX: 0,
    offsetY: 0,
    zoom: 0
  };
  drawBox(ctx, node, borderWidth, dragZoom, padding);
  ok(true, JSON.stringify([ctx, fillRectValues, strokeRectValues]));
  equal(ctx.fillStyle, "hsl(210,60%,70%)", "The fillStyle is set");
  equal(ctx.strokeStyle, "hsl(210,60%,35%)", "The strokeStyle is set");
  equal(ctx.lineWidth, 1, "The lineWidth is set");
  deepEqual(fillRectValues, [10.5, 20.5, 49, 69], "Draws a filled rectangle");
  deepEqual(strokeRectValues, [10.5, 20.5, 49, 69], "Draws a stroked rectangle");

  dragZoom.zoom = 0.5;

  drawBox(ctx, node, borderWidth, dragZoom, padding);
  ok(true, JSON.stringify([ctx, fillRectValues, strokeRectValues]));
  deepEqual(fillRectValues, [15.5, 30.5, 74, 104],
    "Draws a zoomed filled rectangle");
  deepEqual(strokeRectValues, [15.5, 30.5, 74, 104],
    "Draws a zoomed stroked rectangle");

  dragZoom.offsetX = 110;
  dragZoom.offsetY = 130;

  drawBox(ctx, node, borderWidth, dragZoom, padding);
  deepEqual(fillRectValues, [-94.5, -99.5, 74, 104],
    "Draws a zoomed and offset filled rectangle");
  deepEqual(strokeRectValues, [-94.5, -99.5, 74, 104],
    "Draws a zoomed and offset stroked rectangle");
});
