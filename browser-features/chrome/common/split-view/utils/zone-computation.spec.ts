/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import assert from "node:assert/strict";
import {
  computeDropZone,
  zoneToLayout,
  zoneToTabOrder,
} from "./zone-computation.ts";

// computeDropZone — default threshold 0.33
{
  // Left zone: relX < 0.33
  assert.equal(computeDropZone(0.1, 0.5), "left");
  assert.equal(computeDropZone(0, 0.5), "left");
  assert.equal(computeDropZone(0.32, 0.5), "left");

  // Right zone: relX > 0.67
  assert.equal(computeDropZone(0.9, 0.5), "right");
  assert.equal(computeDropZone(1.0, 0.5), "right");
  assert.equal(computeDropZone(0.68, 0.5), "right");

  // Top zone: relY < 0.33 (and relX not in edge zones)
  assert.equal(computeDropZone(0.5, 0.1), "top");
  assert.equal(computeDropZone(0.5, 0), "top");
  assert.equal(computeDropZone(0.5, 0.32), "top");

  // Bottom zone: relY > 0.67 (and relX not in edge zones)
  assert.equal(computeDropZone(0.5, 0.9), "bottom");
  assert.equal(computeDropZone(0.5, 1.0), "bottom");
  assert.equal(computeDropZone(0.5, 0.68), "bottom");

  // Center zone: relX and relY both within threshold boundaries
  assert.equal(computeDropZone(0.5, 0.5), "center");
  assert.equal(computeDropZone(0.4, 0.6), "center");
  assert.equal(computeDropZone(0.6, 0.4), "center");
}

// computeDropZone — custom threshold
{
  assert.equal(computeDropZone(0.1, 0.5, 0.15), "left");
  assert.equal(computeDropZone(0.2, 0.5, 0.15), "center"); // past threshold, falls into center
  assert.equal(computeDropZone(0.5, 0.9, 0.33), "bottom");
  assert.equal(computeDropZone(0.5, 0.5, 0.33), "center"); // center with 33% threshold
}

// X-axis priority over Y-axis
{
  // At corner (relX < threshold, relY < threshold), X wins → "left"
  assert.equal(computeDropZone(0.1, 0.1), "left");
  assert.equal(computeDropZone(0.9, 0.1), "right");
}

// zoneToLayout
{
  assert.equal(zoneToLayout("left"), "horizontal");
  assert.equal(zoneToLayout("right"), "horizontal");
  assert.equal(zoneToLayout("top"), "vertical");
  assert.equal(zoneToLayout("bottom"), "vertical");
  assert.equal(zoneToLayout("center"), "horizontal"); // center uses default layout
}

// zoneToTabOrder
{
  const sel = { id: "sel" };
  const drag = { id: "drag" };

  // right/bottom: selected first (left/top pane)
  assert.deepEqual(zoneToTabOrder("right", sel, drag), [sel, drag]);
  assert.deepEqual(zoneToTabOrder("bottom", sel, drag), [sel, drag]);

  // left/top: dragged first (becomes left/top pane)
  assert.deepEqual(zoneToTabOrder("left", sel, drag), [drag, sel]);
  assert.deepEqual(zoneToTabOrder("top", sel, drag), [drag, sel]);
}

console.log("zone-computation.spec: ok");
