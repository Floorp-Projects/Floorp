/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");
  const {
    sanitizeString,
    isGrip,
    wrapRender,
  } = require("./rep-utils");
  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders a long string grip.
   */
  const LongStringRep = React.createClass({
    displayName: "LongStringRep",

    propTypes: {
      useQuotes: React.PropTypes.bool,
      style: React.PropTypes.object,
    },

    getDefaultProps: function () {
      return {
        useQuotes: true,
      };
    },

    render: wrapRender(function () {
      let {
        cropLimit,
        member,
        object,
        style,
        useQuotes
      } = this.props;
      let {fullText, initial, length} = object;

      let config = {className: "objectBox objectBox-string"};
      if (style) {
        config.style = style;
      }

      let string = member && member.open
        ? fullText || initial
        : initial.substring(0, cropLimit);

      if (string.length < length) {
        string += "\u2026";
      }
      let formattedString = useQuotes ? `"${string}"` : string;
      return span(config, sanitizeString(formattedString));
    }),
  });

  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return object.type === "longString";
  }

  // Exports from this module
  exports.LongStringRep = {
    rep: LongStringRep,
    supportsObject: supportsObject,
  };
});
