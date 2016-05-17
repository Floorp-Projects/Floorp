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
  const DOM = React.DOM;

  /**
   * Renders a box for given object.
   */
  const ObjectBox = React.createClass({
    displayName: "ObjectBox",

    render: function () {
      let className = this.props.className;
      let boxClassName = className ? " objectBox-" + className : "";

      return (
        DOM.span({className: "objectBox" + boxClassName, role: "presentation"},
          this.props.children
        )
      );
    }
  });

  // Exports from this module
  exports.ObjectBox = ObjectBox;
});
