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
  const { createFactories } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));
  const { Caption } = createFactories(require("./caption"));

  // Shortcuts
  const DOM = React.DOM;

  /**
   * Renders an array. The array is enclosed by left and right bracket
   * and the max number of rendered items depends on the current mode.
   */
  let ArrayRep = React.createClass({
    displayName: "ArrayRep",

    getTitle: function (object, context) {
      return "[" + object.length + "]";
    },

    arrayIterator: function (array, max) {
      let items = [];
      let delim;

      for (let i = 0; i < array.length && i < max; i++) {
        try {
          let value = array[i];

          delim = (i == array.length - 1 ? "" : ", ");

          if (value === array) {
            items.push(Reference({
              key: i,
              object: value,
              delim: delim
            }));
          } else {
            items.push(ItemRep({
              key: i,
              object: value,
              delim: delim
            }));
          }
        } catch (exc) {
          items.push(ItemRep({
            object: exc,
            delim: delim,
            key: i
          }));
        }
      }

      if (array.length > max) {
        let objectLink = this.props.objectLink || DOM.span;
        items.push(Caption({
          key: "more",
          object: objectLink({
            object: this.props.object
          }, (array.length - max) + " more…")
        }));
      }

      return items;
    },

    /**
     * Returns true if the passed object is an array with additional (custom)
     * properties, otherwise returns false. Custom properties should be
     * displayed in extra expandable section.
     *
     * Example array with a custom property.
     * let arr = [0, 1];
     * arr.myProp = "Hello";
     *
     * @param {Array} array The array object.
     */
    hasSpecialProperties: function (array) {
      function isInteger(x) {
        let y = parseInt(x, 10);
        if (isNaN(y)) {
          return false;
        }
        return x === y.toString();
      }

      let props = Object.getOwnPropertyNames(array);
      for (let i = 0; i < props.length; i++) {
        let p = props[i];

        // Valid indexes are skipped
        if (isInteger(p)) {
          continue;
        }

        // Ignore standard 'length' property, anything else is custom.
        if (p != "length") {
          return true;
        }
      }

      return false;
    },

    // Event Handlers

    onToggleProperties: function (event) {
    },

    onClickBracket: function (event) {
    },

    render: function () {
      let mode = this.props.mode || "short";
      let object = this.props.object;
      let items;

      if (mode == "tiny") {
        let isEmpty = object.length === 0;
        items = DOM.span({className: "length"}, isEmpty ? "" : object.length);
      } else {
        let max = (mode == "short") ? 3 : 300;
        items = this.arrayIterator(object, max);
      }

      let objectLink = this.props.objectLink || DOM.span;

      return (
        ObjectBox({
          className: "array"},
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
          DOM.span({
            className: "arrayProperties",
            role: "group"}
          )
        )
      );
    },
  });

  /**
   * Renders array item. Individual values are separated by a comma.
   */
  let ItemRep = React.createFactory(React.createClass({
    displayName: "ItemRep",

    render: function () {
      const { Rep } = createFactories(require("./rep"));

      let object = this.props.object;
      let delim = this.props.delim;
      return (
        DOM.span({},
          Rep({object: object}),
          delim
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
      let tooltip = "Circular reference";
      return (
        DOM.span({title: tooltip},
          "[…]")
      );
    }
  }));

  function supportsObject(object, type) {
    return Array.isArray(object) ||
      Object.prototype.toString.call(object) === "[object Arguments]";
  }

  // Exports from this module
  exports.ArrayRep = {
    rep: ArrayRep,
    supportsObject: supportsObject
  };
});
