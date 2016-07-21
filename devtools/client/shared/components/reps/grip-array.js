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
  const { createFactories, isGrip } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));
  const { Caption } = createFactories(require("./caption"));

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders an array. The array is enclosed by left and right bracket
   * and the max number of rendered items depends on the current mode.
   */
  let GripArray = React.createClass({
    displayName: "GripArray",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
      provider: React.PropTypes.object,
    },

    getLength: function (grip) {
      return grip.preview ? grip.preview.length : 0;
    },

    getTitle: function (object, context) {
      let objectLink = this.props.objectLink || span;
      if (this.props.mode != "tiny") {
        return objectLink({
          object: object
        }, object.class);
      }
      return "";
    },

    arrayIterator: function (grip, max) {
      let items = [];

      if (!grip.preview || !grip.preview.length) {
        return items;
      }

      let array = grip.preview.items;
      if (!array) {
        return items;
      }

      let delim;
      let provider = this.props.provider;

      for (let i = 0; i < array.length && i <= max; i++) {
        try {
          let itemGrip = array[i];
          let value = provider ? provider.getValue(itemGrip) : itemGrip;

          delim = (i == array.length - 1 ? "" : ", ");

          if (value === array) {
            items.push(Reference({
              key: i,
              object: value,
              delim: delim}
            ));
          } else {
            items.push(GripArrayItem(Object.assign({}, this.props, {
              key: i,
              object: value,
              delim: delim}
            )));
          }
        } catch (exc) {
          items.push(GripArrayItem(Object.assign({}, this.props, {
            object: exc,
            delim: delim,
            key: i}
          )));
        }
      }

      if (array.length > max) {
        items.pop();
        let objectLink = this.props.objectLink || span;
        items.push(Caption({
          key: "more",
          object: objectLink({
            object: this.props.object
          }, "more…")
        }));
      }

      return items;
    },

    render: function () {
      let mode = this.props.mode || "short";
      let object = this.props.object;

      let items;

      if (mode == "tiny") {
        let objectLength = this.getLength(object);
        let isEmpty = objectLength === 0;
        items = span({className: "length"}, isEmpty ? "" : objectLength);
      } else {
        let max = (mode == "short") ? 3 : 300;
        items = this.arrayIterator(object, max);
      }

      let objectLink = this.props.objectLink || span;
      let title = this.getTitle(object);

      return (
        ObjectBox({
          className: "array"},
          title,
          objectLink({
            className: "arrayLeftBracket",
            role: "presentation",
            object: object
          }, "["),
          items,
          objectLink({
            className: "arrayRightBracket",
            role: "presentation",
            object: object
          }, "]"),
          span({
            className: "arrayProperties",
            role: "group"}
          )
        )
      );
    },
  });

  /**
   * Renders array item. Individual values are separated by
   * a delimiter (a comma by default).
   */
  let GripArrayItem = React.createFactory(React.createClass({
    displayName: "GripArrayItem",

    propTypes: {
      delim: React.PropTypes.string,
    },

    render: function () {
      let { Rep } = createFactories(require("./rep"));

      return (
        span({},
          Rep(Object.assign({}, this.props, {
            mode: "tiny"
          })),
          this.props.delim
        )
      );
    }
  }));

  /**
   * Renders cycle references in an array.
   */
  let Reference = React.createFactory(React.createClass({
    displayName: "Reference",

    render: function () {
      return (
        span({title: "Circular reference"},
          "[…]"
        )
      );
    }
  }));

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (grip.preview && grip.preview.kind == "ArrayLike");
  }

  // Exports from this module
  exports.GripArray = {
    rep: GripArray,
    supportsObject: supportsObject
  };
});
