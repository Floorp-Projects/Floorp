/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");
  const { createFactories, cropMultipleLines } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));

  /**
   * Renders a string. String value is enclosed within quotes.
   */
  const StringRep = React.createClass({
    displayName: "StringRep",

    render: function () {
      let text = this.props.object;
      let member = this.props.member;
      if (member && member.open) {
        return (
          ObjectBox({className: "string"},
            "\"" + text + "\""
          )
        );
      }

      return (
        ObjectBox({className: "string"},
          "\"" + cropMultipleLines(text) + "\""
        )
      );
    },
  });

  // Helpers

  function supportsObject(object, type) {
    return (type == "string");
  }

  // Exports from this module

  exports.StringRep = {
    rep: StringRep,
    supportsObject: supportsObject,
  };
});
