/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("react");

// Load all existing rep templates
const { Undefined } = require("./undefined");
const { Null } = require("./null");
const { StringRep } = require("./string");
const { Number } = require("./number");
const { ArrayRep } = require("./array");
const { Obj } = require("./object");

// List of all registered template.
// XXX there should be a way for extensions to register a new
// or modify an existing rep.
var reps = [Undefined, Null, StringRep, Number, ArrayRep, Obj];
var defaultRep;

/**
 * Generic rep that is using for rendering native JS types or an object.
 * The right template used for rendering is picked automatically according
 * to the current value type. The value must be passed is as 'object'
 * property.
 */
const Rep = React.createClass({
  displayName: "Rep",

  render: function() {
    var rep = getRep(this.props.object);
    return rep(this.props);
  },
});

// Helpers

/**
 * Return a rep object that is responsible for rendering given
 * object.
 *
 * @param object {Object} Object to be rendered in the UI. This
 * can be generic JS object as well as a grip (handle to a remote
 * debuggee object).
 */
function getRep(object) {
  var type = typeof(object);
  if (type == "object" && object instanceof String) {
    type = "string";
  }

  if (isGrip(object)) {
    type = object.class;
  }

  for (var i=0; i<reps.length; i++) {
    var rep = reps[i];
    try {
      // supportsObject could return weight (not only true/false
      // but a number), which would allow to priorities templates and
      // support better extensibility.
      if (rep.supportsObject(object, type)) {
        return React.createFactory(rep.rep);
      }
    }
    catch (err) {
      console.error("reps.getRep; EXCEPTION ", err, err);
    }
  }

  return React.createFactory(defaultRep.rep);
}

function isGrip(object) {
  return object && object.actor;
}

// Exports from this module
exports.Rep = Rep;
});
