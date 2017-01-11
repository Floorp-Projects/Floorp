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
   * Renders a Infinity object
   */
  const InfinityRep = React.createClass({
    displayName: "Infinity",

    propTypes: {
      object: React.PropTypes.object.isRequired,
    },

    render: wrapRender(function () {
      return (
        span({className: "objectBox objectBox-number"},
          this.props.object.type
        )
      );
    })
  });

  function supportsObject(object, type) {
    return type == "Infinity" || type == "-Infinity";
  }

  // Exports from this module
  exports.InfinityRep = {
    rep: InfinityRep,
    supportsObject: supportsObject
  };
});
