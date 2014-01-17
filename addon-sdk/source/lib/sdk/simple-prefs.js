/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "experimental"
};

const { emit, off } = require("./event/core");
const { PrefsTarget } = require("./preferences/event-target");
const { id } = require("./self");
const { on } = require("./system/events");

const ADDON_BRANCH = "extensions." + id + ".";
const BUTTON_PRESSED = id + "-cmdPressed";

const target = PrefsTarget({ branchName: ADDON_BRANCH });

// Listen to clicks on buttons
function buttonClick({ data }) {
  emit(target, data);
}
on(BUTTON_PRESSED, buttonClick);

module.exports = target;
