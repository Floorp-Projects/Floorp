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
  const { createFactories, isGrip } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));
  const { getFileName } = require("./url");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders DOM document object.
   */
  let Document = React.createClass({
    displayName: "Document",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    getLocation: function (grip) {
      let location = grip.preview.location;
      return location ? getFileName(location) : "";
    },

    getTitle: function (win, context) {
      return "document";
    },

    getTooltip: function (doc) {
      return doc.location.href;
    },

    render: function () {
      let grip = this.props.object;

      return (
        ObjectBox({className: "object"},
          span({className: "objectPropValue"},
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

    return (object.preview && type == "HTMLDocument");
  }

  // Exports from this module
  exports.Document = {
    rep: Document,
    supportsObject: supportsObject
  };
});
