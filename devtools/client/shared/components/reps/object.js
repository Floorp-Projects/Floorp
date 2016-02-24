/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));
  const { Caption } = createFactories(require("./caption"));

  // Shortcuts
  const DOM = React.DOM;

  /**
   * Renders an object. An object is represented by a list of its
   * properties enclosed in curly brackets.
   */
  const Obj = React.createClass({
    displayName: "Obj",

    render: function() {
      let object = this.props.object;
      let props = this.shortPropIterator(object);

      return (
        ObjectBox({className: "object"},
          DOM.span({className: "objectTitle"}, this.getTitle(object)),
          DOM.span({className: "objectLeftBrace", role: "presentation"}, "{"),
          props,
          DOM.span({className: "objectRightBrace"}, "}")
        )
      );
    },

    getTitle: function() {
      return "";
    },

    longPropIterator: function(object) {
      try {
        return this.propIterator(object, 100);
      } catch (err) {
        console.error(err);
      }
      return [];
    },

    shortPropIterator: function(object) {
      try {
        return this.propIterator(object, 3);
      } catch (err) {
        console.error(err);
      }
      return [];
    },

    propIterator: function(object, max) {
      function isInterestingProp(t, value) {
        return (t == "boolean" || t == "number" || (t == "string" && value) ||
          (t == "object" && value && value.toString));
      }

      // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
      if (Object.prototype.toString.call(object) === "[object Generator]") {
        object = Object.getPrototypeOf(object);
      }

      // Object members with non-empty values are preferred since it gives the
      // user a better overview of the object.
      let props = [];
      this.getProps(props, object, max, isInterestingProp);

      if (props.length <= max) {
        // There are not enough props yet (or at least, not enough props to
        // be able to know whether we should print "more..." or not).
        // Let's display also empty members and functions.
        this.getProps(props, object, max, function(t, value) {
          return !isInterestingProp(t, value);
        });
      }

      if (props.length > max) {
        props.pop();
        props.push(Caption({
          key: "more",
          object: "more...",
        }));
      } else if (props.length > 0) {
        // Remove the last comma.
        props[props.length - 1] = React.cloneElement(
          props[props.length - 1], { delim: "" });
      }

      return props;
    },

    getProps: function(props, object, max, filter) {
      max = max || 3;
      if (!object) {
        return [];
      }

      let mode = this.props.mode;

      try {
        for (let name in object) {
          if (props.length > max) {
            return [];
          }

          let value;
          try {
            value = object[name];
          } catch (exc) {
            continue;
          }

          let t = typeof value;
          if (filter(t, value)) {
            props.push(PropRep({
              key: name,
              mode: mode,
              name: name,
              object: value,
              equal: ": ",
              delim: ", ",
            }));
          }
        }
      } catch (err) {
        console.error(err);
      }

      return [];
    },
  });

  /**
   * Renders object property, name-value pair.
   */
  let PropRep = React.createFactory(React.createClass({
    displayName: "PropRep",

    render: function() {
      let { Rep } = createFactories(require("./rep"));
      let object = this.props.object;
      let mode = this.props.mode;

      return (
        DOM.span({},
          DOM.span({
            "className": "nodeName"},
            this.props.name
          ),
          DOM.span({
            "className": "objectEqual",
            role: "presentation"},
            this.props.equal
          ),
          Rep({
            object: object,
            mode: mode
          }),
          DOM.span({
            "className": "objectComma",
            role: "presentation"},
            this.props.delim
          )
        )
      );
    }
  }));

  function supportsObject(object, type) {
    return true;
  }

  // Exports from this module

  exports.Obj = {
    rep: Obj,
    supportsObject: supportsObject
  };
});
