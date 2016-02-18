/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("devtools/client/shared/vendor/react");
const { createFactories } = require("./rep-utils");
const { ObjectBox } = createFactories(require("./object-box"));

/**
 * Renders a string. String value is enclosed within quotes.
 */
const StringRep = React.createClass({
  displayName: "StringRep",

  render: function() {
    var text = this.props.object;
    var member = this.props.member;
    if (member && member.open) {
      return (
        ObjectBox({className: "string"},
          "\"" + text + "\""
        )
      )
    } else {
      return (
        ObjectBox({className: "string"},
          "\"" + cropMultipleLines(text) + "\""
        )
      )
    }
  },
});

// Helpers

function escapeNewLines(value) {
  return value.replace(/\r/gm, "\\r").replace(/\n/gm, "\\n");
};

function cropMultipleLines(text, limit) {
  return escapeNewLines(cropString(text, limit));
};

function cropString(text, limit, alternativeText) {
  if (!alternativeText) {
    alternativeText = "...";
  }

  // Make sure it's a string.
  text = text + "";

  // Use default limit if necessary.
  if (!limit) {
    limit = 50;
  }

  // Crop the string only if a limit is actually specified.
  if (limit <= 0) {
    return text;
  }

  // Set the limit at least to the length of the alternative text
  // plus one character of the original text.
  if (limit <= alternativeText.length) {
    limit = alternativeText.length + 1;
  }

  var halfLimit = (limit - alternativeText.length) / 2;

  if (text.length > limit) {
    return text.substr(0, Math.ceil(halfLimit)) + alternativeText +
      text.substr(text.length - Math.floor(halfLimit));
  }

  return text;
};

function isCropped(value) {
  var cropLength = 50;
  return typeof(value) == "string" && value.length > cropLength;
}

function supportsObject(object, type) {
  return (type == "string");
}

// Exports from this module

exports.StringRep = {
  rep: StringRep,
  supportsObject: supportsObject,
  isCropped: isCropped
};

});
