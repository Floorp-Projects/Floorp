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
 *   - label: the label used in the waterfall to identify the marker
 *   - colorName: the name of the DevTools color used for this marker. If adding
 *                a new color, be sure to check that there's an entry for
 *                `.marker-details-bullet.{COLORNAME}` for the equivilent entry
 *                in ./browser/themes/shared/devtools/performance.inc.css
 *                https://developer.mozilla.org/en-US/docs/Tools/DevToolsColors
 *
 * Whenever this is changed, browser_timeline_waterfall-styles.js *must* be
 * updated as well.
 */
const TIMELINE_BLUEPRINT = {
  "Styles": {
    group: 0,
    colorName: "highlight-pink",
    label: L10N.getStr("timeline.label.styles2")
  },
  "Reflow": {
    group: 0,
    colorName: "highlight-pink",
    label: L10N.getStr("timeline.label.reflow2")
  },
  "Paint": {
    group: 0,
    colorName: "highlight-green",
    label: L10N.getStr("timeline.label.paint")
  },
  "DOMEvent": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.domevent")
  },
  "Javascript": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.javascript2")
  },
  "Parse HTML": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.parseHTML")
  },
  "Parse XML": {
    group: 1,
    colorName: "highlight-lightorange",
    label: L10N.getStr("timeline.label.parseXML")
  },
  "ConsoleTime": {
    group: 2,
    colorName: "highlight-bluegrey",
    label: L10N.getStr("timeline.label.consoleTime")
  },
  "GarbageCollection": {
    group: 1,
    colorName: "highlight-red",
    label: L10N.getStr("timeline.label.garbageCollection")
  },
};

// Exported symbols.
exports.L10N = L10N;
exports.TIMELINE_BLUEPRINT = TIMELINE_BLUEPRINT;
