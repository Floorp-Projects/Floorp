/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const React = require("devtools/client/shared/vendor/react");

  // Reps
  const { isGrip, getURLDisplayString } = require("./rep-utils");

  // Shortcuts
  const DOM = React.DOM;

  /**
   * Renders a grip representing CSSStyleSheet
   */
  let StyleSheet = React.createClass({
    displayName: "object",

    propTypes: {
      object: React.PropTypes.object.isRequired,
    },

    getTitle: function (grip) {
      let title = "StyleSheet ";
      if (this.props.objectLink) {
        return DOM.span({className: "objectBox"},
          this.props.objectLink({
            object: grip
          }, title)
        );
      }
      return title;
    },

    getLocation: function (grip) {
      // Embedded stylesheets don't have URL and so, no preview.
      let url = grip.preview ? grip.preview.url : "";
      return url ? getURLDisplayString(url) : "";
    },

    render: function () {
      let grip = this.props.object;

      return (
        DOM.span({className: "objectBox objectBox-object"},
          this.getTitle(grip),
          DOM.span({className: "objectPropValue"},
            this.getLocation(grip)
          )
        )
      );
    },
  });

  // Registration

  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }

    return (type == "CSSStyleSheet");
  }

  // Exports from this module

  exports.StyleSheet = {
    rep: StyleSheet,
    supportsObject: supportsObject
  };
});
