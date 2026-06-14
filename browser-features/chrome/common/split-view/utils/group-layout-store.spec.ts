/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import assert from "node:assert/strict";
import {
  getGroupLayoutFromStore,
  getGroupPaneSizesFromStore,
  parseGroupLayoutStore,
  upsertGroupLayoutInStore,
  upsertGroupPaneSizesInStore,
} from "./group-layout-store.ts";

{
  const store = parseGroupLayoutStore(
    JSON.stringify({
      groups: [
        {
          groupId: "alpha",
          layout: "grid-2x2",
          paneSizes: {
            flexRatios: [0.6, 0.4],
            gridColRatio: 0.6,
            gridRowRatio: 0.3,
          },
        },
        { groupId: "beta", layout: "vertical" },
      ],
    }),
  );
  assert.equal(getGroupLayoutFromStore(store, "alpha"), "grid-2x2");
  assert.equal(getGroupLayoutFromStore(store, "beta"), "vertical");
  assert.equal(getGroupLayoutFromStore(store, "missing"), null);
  assert.deepEqual(getGroupPaneSizesFromStore(store, "alpha"), {
    flexRatios: [0.6, 0.4],
    gridColRatio: 0.6,
    gridRowRatio: 0.3,
  });
  assert.equal(getGroupPaneSizesFromStore(store, "beta"), null);
}

{
  const start = parseGroupLayoutStore(
    JSON.stringify({
      groups: [{ groupId: "alpha", layout: "horizontal" }],
    }),
  );
  const next = upsertGroupLayoutInStore(
    upsertGroupLayoutInStore(start, "beta", "grid-3pane-left-main"),
    "alpha",
    "grid-2x2",
  );
  assert.equal(getGroupLayoutFromStore(next, "alpha"), "grid-2x2");
  assert.equal(getGroupLayoutFromStore(next, "beta"), "grid-3pane-left-main");
}

// Verify new 3-pane layouts survive round-trip through parseGroupLayoutStore
{
  for (
    const layout of [
      "grid-3pane-right-main",
      "grid-3pane-top-main",
      "grid-3pane-bottom-main",
    ] as const
  ) {
    const raw = JSON.stringify({
      groups: [{ groupId: "g", layout }],
    });
    const store = parseGroupLayoutStore(raw);
    assert.equal(getGroupLayoutFromStore(store, "g"), layout);
  }
}

// Verify upsert works for new 3-pane layouts
{
  const start = parseGroupLayoutStore(
    JSON.stringify({
      groups: [{ groupId: "a", layout: "horizontal" }],
    }),
  );
  const next = upsertGroupLayoutInStore(start, "a", "grid-3pane-right-main");
  assert.equal(getGroupLayoutFromStore(next, "a"), "grid-3pane-right-main");
}

{
  const start = parseGroupLayoutStore(
    JSON.stringify({
      groups: [{ groupId: "alpha", layout: "horizontal" }],
    }),
  );
  const next = upsertGroupPaneSizesInStore(start, "alpha", {
    flexRatios: [0.2, 0.3, 0.5],
    gridColRatio: 0.55,
    gridRowRatio: 0.45,
  });
  assert.deepEqual(getGroupPaneSizesFromStore(next, "alpha"), {
    flexRatios: [0.2, 0.3, 0.5],
    gridColRatio: 0.55,
    gridRowRatio: 0.45,
  });
}

{
  const invalid = parseGroupLayoutStore(
    JSON.stringify({
      groups: [
        { groupId: "alpha", layout: "nope" },
        { groupId: 1, layout: "horizontal" },
        {
          groupId: "beta",
          layout: "horizontal",
          paneSizes: {
            flexRatios: "bad",
            gridColRatio: 0.5,
            gridRowRatio: 0.5,
          },
        },
      ],
    }),
  );
  assert.deepEqual(invalid, {
    groups: [{ groupId: "beta", layout: "horizontal", paneSizes: undefined }],
  });
}

console.log("group-layout-store.spec: ok");
