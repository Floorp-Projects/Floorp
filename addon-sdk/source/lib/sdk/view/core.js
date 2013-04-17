/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

var { Ci } = require("chrome");
var method = require("method/core");

// Returns DOM node associated with a view for
// the given `value`. If `value` has no view associated
// it returns `null`. You can implement this method for
// this type to define what the result should be for it.
let getNodeView = method("getNodeView");
getNodeView.define(function(value) {
  if (value instanceof Ci.nsIDOMNode)
    return value;
  return null;
});

exports.getNodeView = getNodeView;

let getActiveView = method("getActiveView");
exports.getActiveView = getActiveView;
