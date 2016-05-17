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
  const { ObjectLink } = createFactories(require("./object-link"));
  const { StringRep } = require("./string");

  // Shortcuts
  const { span } = React.DOM;
  const { rep: StringRepFactory } = createFactories(StringRep);

  /**
   * Renders DOM attribute
   */
  let Attribute = React.createClass({
    displayName: "Attr",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    getTitle: function (grip) {
      return grip.preview.nodeName;
    },

    render: function () {
      let grip = this.props.object;
      let value = grip.preview.value;

      return (
        ObjectLink({className: "Attr"},
          span({},
            span({className: "attrTitle"},
              this.getTitle(grip)
            ),
            span({className: "attrEqual"},
              "="
            ),
            StringRepFactory({object: value})
          )
        )
      );
    },
  });

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (type == "Attr" && grip.preview);
  }

  exports.Attribute = {
    rep: Attribute,
    supportsObject: supportsObject
  };
});
