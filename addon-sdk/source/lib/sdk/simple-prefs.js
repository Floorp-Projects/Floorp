/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "experimental"
};

const { emit, off } = require("./event/core");
const { when: unload } = require("./system/unload");
const { PrefsTarget } = require("./preferences/event-target");
const { id } = require("./self");
const { preferencesBranch } = require('@loader/options');

const observers = require("./deprecated/observer-service");

const ADDON_BRANCH = "extensions." + preferencesBranch + ".";
const BUTTON_PRESSED = id + "-cmdPressed";

const target = PrefsTarget({ branchName: ADDON_BRANCH });

// Listen to clicks on buttons
function buttonClick(subject, data) {
  emit(target, data);
}
observers.add(BUTTON_PRESSED, buttonClick);

// Make sure we cleanup listeners on unload.
unload(function() {
  observers.remove(BUTTON_PRESSED, buttonClick);
});

module.exports = target;
