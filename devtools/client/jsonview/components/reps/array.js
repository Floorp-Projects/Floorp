/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("react");
const { createFactories } = require("./rep-utils");
const { Rep } = createFactories(require("./rep"));
const { ObjectBox } = createFactories(require("./object-box"));
const { Caption } = createFactories(require("./caption"));

// Shortcuts
const DOM = React.DOM;

/**
 * Renders an array. The array is enclosed by left and right bracket
 * and the max number of rendered items depends on the current mode.
 */
var ArrayRep = React.createClass({
  displayName: "ArrayRep",

  render: function() {
    var mode = this.props.mode || "short";
    var object = this.props.object;
    var hasTwisty = this.hasSpecialProperties(object);

    var items;

    if (mode == "tiny") {
      items = object.length;
    } else {
      var max = (mode == "short") ? 3 : 300;
      items = this.arrayIterator(object, max);
    }

    return (
      ObjectBox({className: "array", onClick: this.onToggleProperties},
        DOM.a({className: "objectLink", onclick: this.onClickBracket},
          DOM.span({className: "arrayLeftBracket", role: "presentation"}, "[")
        ),
        items,
        DOM.a({className: "objectLink", onclick: this.onClickBracket},
          DOM.span({className: "arrayRightBracket", role: "presentation"}, "]")
        ),
        DOM.span({className: "arrayProperties", role: "group"})
      )
    )
  },

  getTitle: function(object, context) {
    return "[" + object.length + "]";
  },

  arrayIterator: function(array, max) {
    var items = [];

    for (var i=0; i<array.length && i<=max; i++) {
      try {
        var delim = (i == array.length-1 ? "" : ", ");
        var value = array[i];

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
        items.push(ItemRep({object: exc, delim: delim, key: i}));
      }
    }

    if (array.length > max + 1) {
      items.pop();
      items.push(Caption({
        key: "more",
        object: Locale.$STR("jsonViewer.reps.more"),
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
  hasSpecialProperties: function(array) {
    function isInteger(x) {
      var y = parseInt(x, 10);
      if (isNaN(y)) {
        return false;
      }
      return x === y.toString();
    }

    var n = 0;
    var props = Object.getOwnPropertyNames(array);
    for (var i=0; i<props.length; i++) {
      var p = props[i];

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

  onToggleProperties: function(event) {
  },

  onClickBracket: function(event) {
  }
});

/**
 * Renders array item. Individual values are separated by a comma.
 */
var ItemRep = React.createFactory(React.createClass({
  displayName: "ItemRep",

  render: function(){
    var object = this.props.object;
    var delim = this.props.delim;
    return (
      DOM.span({},
        Rep({object: object}),
        delim
      )
    )
  }
}));

/**
 * Renders cycle references in an array.
 */
var Reference = React.createFactory(React.createClass({
  displayName: "Reference",

  render: function(){
    var tooltip = Locale.$STR("jsonView.reps.reference");
    return (
      span({title: tooltip},
        "[...]")
    )
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
