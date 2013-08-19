/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const models = exports.models = new WeakMap();
const views = exports.views = new WeakMap();
exports.buttons = new WeakMap();

exports.viewsFor = function viewsFor(sidebar) views.get(sidebar);
exports.modelFor = function modelFor(sidebar) models.get(sidebar);
