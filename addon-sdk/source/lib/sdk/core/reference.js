/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const method = require("../../method/core");
const { Class } = require("./heritage");

// Object that inherit or mix WeakRefence inn will register
// weak observes for system notifications.
const WeakReference = Class({});
exports.WeakReference = WeakReference;


// If `isWeak(object)` is `true` observer installed
// for such `object` will be weak, meaning that it will
// be GC-ed if nothing else but observer is observing it.
// By default everything except `WeakReference` will return
// `false`.
const isWeak = method("reference/weak?");
exports.isWeak = isWeak;

isWeak.define(Object, _ => false);
isWeak.define(WeakReference, _ => true);
