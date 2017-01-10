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
  const {
    createFactories,
    isGrip,
    wrapRender,
  } = require("./rep-utils");
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

    render: wrapRender(function () {
      let object = this.props.object;
      let value = object.preview.value;
      let objectLink = this.props.objectLink || span;

      return (
        objectLink({className: "objectLink-Attr", object},
          span({},
            span({className: "attrTitle"},
              this.getTitle(object)
            ),
            span({className: "attrEqual"},
              "="
            ),
            StringRepFactory({object: value})
          )
        )
      );
    }),
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
