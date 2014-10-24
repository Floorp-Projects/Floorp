/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

/**
 * Localization convenience methods.
 */
const STRINGS_URI = "chrome://browser/locale/devtools/timeline.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * A simple schema for mapping markers to the timeline UI. The keys correspond
 * to marker names, while the values are objects with the following format:
 *   - group: the row index in the timeline overview graph; multiple markers
 *            can be added on the same row. @see <overview.js/buildGraphImage>
 *   - fill: a fill color used when drawing the marker
 *   - stroke: a stroke color used when drawing the marker
 *   - label: the label used in the waterfall to identify the marker
 *
 * Whenever this is changed, browser_timeline_waterfall-styles.js *must* be
 * updated as well.
 */
const TIMELINE_BLUEPRINT = {
  "Styles": {
    group: 0,
    fill: "hsl(285,50%,68%)",
    stroke: "hsl(285,50%,48%)",
    label: L10N.getStr("timeline.label.styles")
  },
  "Reflow": {
    group: 2,
    fill: "hsl(104,57%,71%)",
    stroke: "hsl(104,57%,51%)",
    label: L10N.getStr("timeline.label.reflow")
  },
  "Paint": {
    group: 1,
    fill: "hsl(39,82%,69%)",
    stroke: "hsl(39,82%,49%)",
    label: L10N.getStr("timeline.label.paint")
  },
  "ConsoleTime": {
    group: 3,
    fill: "hsl(0,0%,80%)",
    stroke: "hsl(0,0%,60%)",
    label: L10N.getStr("timeline.label.consoleTime")
  }
};

// Exported symbols.
exports.L10N = L10N;
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
