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

  const { wrapRender } = require("./rep-utils");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders null value
   */
  const Null = React.createClass({
    displayName: "NullRep",

    render: wrapRender(function () {
      return (
        span({className: "objectBox objectBox-null"},
          "null"
        )
      );
    }),
  });

  function supportsObject(object, type) {
    if (object && object.type && object.type == "null") {
      return true;
    }

    return (object == null);
  }

  // Exports from this module

  exports.Null = {
    rep: Null,
    supportsObject: supportsObject
  };
});
