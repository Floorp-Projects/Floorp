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
  const { PropRep } = createFactories(require("./prop-rep"));
  // Shortcuts
  const { span } = React.DOM;
  /**
   * Renders an object. An object is represented by a list of its
   * properties enclosed in curly brackets.
   */
  const Obj = React.createClass({
    displayName: "Obj",

    propTypes: {
      object: React.PropTypes.object,
      mode: React.PropTypes.string,
    },

    getTitle: function (object) {
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: object
        }, object.class);
      }
      return "Object";
    },

    safePropIterator: function (object, max) {
      max = (typeof max === "undefined") ? 3 : max;
      try {
        return this.propIterator(object, max);
      } catch (err) {
        console.error(err);
      }
      return [];
    },

    propIterator: function (object, max) {
      let isInterestingProp = (t, value) => {
        // Do not pick objects, it could cause recursion.
        return (t == "boolean" || t == "number" || (t == "string" && value));
      };

      // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
      if (Object.prototype.toString.call(object) === "[object Generator]") {
        object = Object.getPrototypeOf(object);
      }

      // Object members with non-empty values are preferred since it gives the
      // user a better overview of the object.
      let props = this.getProps(object, max, isInterestingProp);

      if (props.length <= max) {
        // There are not enough props yet (or at least, not enough props to
        // be able to know whether we should print "more…" or not).
        // Let's display also empty members and functions.
        props = props.concat(this.getProps(object, max, (t, value) => {
          return !isInterestingProp(t, value);
        }));
      }

      if (props.length > max) {
        props.pop();
        let objectLink = this.props.objectLink || span;

        props.push(Caption({
          key: "more",
          object: objectLink({
            object: object
          }, "more…")
        }));
      } else if (props.length > 0) {
        // Remove the last comma.
        props[props.length - 1] = React.cloneElement(
          props[props.length - 1], { delim: "" });
      }

      return props;
    },

    getProps: function (object, max, filter) {
      let props = [];

      max = max || 3;
      if (!object) {
        return props;
      }

      let mode = this.props.mode;

      try {
        for (let name in object) {
          if (props.length > max) {
            return props;
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

      return props;
    },

    render: function () {
      let object = this.props.object;
      let props = this.safePropIterator(object);
      let objectLink = this.props.objectLink || span;

      if (this.props.mode == "tiny" || !props.length) {
        return (
          ObjectBox({className: "object"},
            objectLink({className: "objectTitle"}, this.getTitle())
          )
        );
      }

      return (
        ObjectBox({className: "object"},
          this.getTitle(object),
          objectLink({
            className: "objectLeftBrace",
            role: "presentation",
            object: object
          }, "{"),
          props,
          objectLink({
            className: "objectRightBrace",
            role: "presentation",
            object: object
          }, "}")
        )
      );
    },
  });
  function supportsObject(object, type) {
    return true;
  }

  // Exports from this module
  exports.Obj = {
    rep: Obj,
    supportsObject: supportsObject
  };
});
