/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import assert from "node:assert/strict";
import { getEffectiveSplitViewLayout } from "./utils/effective-layout.ts";

assert.equal(getEffectiveSplitViewLayout("horizontal", 2), "horizontal");
assert.equal(getEffectiveSplitViewLayout("vertical", 3), "vertical");
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-left-main", 3),
  "grid-3pane-left-main",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-left-main", 4),
  "horizontal",
);
assert.equal(getEffectiveSplitViewLayout("grid-2x2", 4), "grid-2x2");
assert.equal(getEffectiveSplitViewLayout("grid-2x2", 3), "horizontal");

// New 3-pane grid layouts: should match when paneCount=3, fallback otherwise
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-right-main", 3),
  "grid-3pane-right-main",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-right-main", 4),
  "horizontal",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-top-main", 3),
  "grid-3pane-top-main",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-top-main", 2),
  "horizontal",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-bottom-main", 3),
  "grid-3pane-bottom-main",
);
assert.equal(
  getEffectiveSplitViewLayout("grid-3pane-bottom-main", 4),
  "horizontal",
);

console.log("layout.spec: ok");
