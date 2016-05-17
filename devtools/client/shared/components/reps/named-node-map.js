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
  const { Caption } = createFactories(require("./caption"));

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Used to render a map of values provided as a grip.
   */
  let NamedNodeMap = React.createClass({

    propTypes: {
      object: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
      provider: React.PropTypes.object,
    },

    className: "NamedNodeMap",

    getLength: function (object) {
      return object.preview.length;
    },

    getTitle: function (object) {
      return object.class ? object.class : "";
    },

    getItems: function (array, max) {
      let items = this.propIterator(array, max);

      items = items.map(item => PropRep(item));

      if (items.length > max + 1) {
        items.pop();
        items.push(Caption({
          key: "more",
          object: "more...",
        }));
      }

      return items;
    },

    propIterator: function (grip, max) {
      max = max || 3;

      let props = [];

      let provider = this.props.provider;
      if (!provider) {
        return props;
      }

      let ownProperties = grip.preview ? grip.preview.ownProperties : [];
      for (let name in ownProperties) {
        if (props.length > max) {
          break;
        }

        let item = ownProperties[name];
        let label = provider.getLabel(item);
        let value = provider.getValue(item);

        props.push(Object.assign({}, this.props, {
          name: label,
          object: value,
          equal: ": ",
          delim: ", ",
        }));
      }

      return props;
    },

    render: function () {
      let grip = this.props.object;
      let mode = this.props.mode;

      let items;
      if (mode == "tiny") {
        items = this.getLength(grip);
      } else {
        let max = (mode == "short") ? 3 : 100;
        items = this.getItems(grip, max);
      }

      return (
        ObjectLink({className: "NamedNodeMap"},
          span({className: "objectTitle"},
            this.getTitle(grip)
          ),
          span({
            className: "arrayLeftBracket",
            role: "presentation"},
            "["
          ),
          items,
          span({
            className: "arrayRightBracket",
            role: "presentation"},
            "]"
          )
        )
      );
    },
  });

  /**
   * Property for a grip object.
   */
  let PropRep = React.createFactory(React.createClass({
    displayName: "PropRep",

    propTypes: {
      equal: React.PropTypes.string,
      delim: React.PropTypes.string,
    },

    render: function () {
      const { Rep } = createFactories(require("./rep"));

      return (
        span({},
          span({
            className: "nodeName"},
            "$prop.name"
          ),
          span({
            className: "objectEqual",
            role: "presentation"},
            this.props.equal
          ),
          Rep(this.props),
          span({
            className: "objectComma",
            role: "presentation"},
            this.props.delim
          )
        )
      );
    }
  }));

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (type == "NamedNodeMap" && grip.preview);
  }

  // Exports from this module
  exports.NamedNodeMap = {
    rep: NamedNodeMap,
    supportsObject: supportsObject
  };
});
