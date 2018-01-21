/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file exposes the Redux reducers of the box model, grid and grid highlighter
// settings.

exports.animations = require("devtools/client/inspector/animation/reducers/animations");
exports.boxModel = require("devtools/client/inspector/boxmodel/reducers/box-model");
exports.changes = require("devtools/client/inspector/changes/reducers/changes");
exports.events = require("devtools/client/inspector/events/reducers/events");
exports.extensionsSidebar = require("devtools/client/inspector/extensions/reducers/sidebar");
exports.flexboxes = require("devtools/client/inspector/flexbox/reducers/flexboxes");
exports.fontOptions = require("devtools/client/inspector/fonts/reducers/font-options");
exports.fonts = require("devtools/client/inspector/fonts/reducers/fonts");
exports.grids = require("devtools/client/inspector/grids/reducers/grids");
exports.highlighterSettings = require("devtools/client/inspector/grids/reducers/highlighter-settings");
