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
   * Renders a symbol.
   */
  const SymbolRep = React.createClass({
    displayName: "SymbolRep",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    render: wrapRender(function () {
      let {object} = this.props;
      let {name} = object;

      return (
        span({className: "objectBox objectBox-symbol"},
          `Symbol(${name || ""})`
        )
      );
    }),
  });

  function supportsObject(object, type) {
    return (type == "symbol");
  }

  // Exports from this module
  exports.SymbolRep = {
    rep: SymbolRep,
    supportsObject: supportsObject,
  };
});
