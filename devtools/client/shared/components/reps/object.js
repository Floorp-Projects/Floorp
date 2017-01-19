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
  const {
    createFactories,
    wrapRender,
  } = require("./rep-utils");
  const { Caption } = createFactories(require("./caption"));
  const { PropRep } = createFactories(require("./prop-rep"));
  const { MODE } = require("./constants");
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
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
      objectLink: React.PropTypes.func,
    },

    getTitle: function (object) {
      let className = object && object.class ? object.class : "Object";
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: object
        }, className);
      }
      return className;
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
      let propsArray = this.getPropsArray(object, max, isInterestingProp);

      if (propsArray.length <= max) {
        // There are not enough props yet (or at least, not enough props to
        // be able to know whether we should print "more…" or not).
        // Let's display also empty members and functions.
        propsArray = propsArray.concat(this.getPropsArray(object, max, (t, value) => {
          return !isInterestingProp(t, value);
        }));
      }

      if (propsArray.length > max) {
        propsArray.pop();
        let objectLink = this.props.objectLink || span;

        propsArray.push(Caption({
          object: objectLink({
            object: object
          }, (Object.keys(object).length - max) + " more…")
        }));
      } else if (propsArray.length > 0) {
        // Remove the last comma.
        propsArray[propsArray.length - 1] = React.cloneElement(
          propsArray[propsArray.length - 1], { delim: "" });
      }

      return propsArray;
    },

    getPropsArray: function (object, max, filter) {
      let propsArray = [];

      max = max || 3;
      if (!object) {
        return propsArray;
      }

      // Hardcode tiny mode to avoid recursive handling.
      let mode = MODE.TINY;

      try {
        for (let name in object) {
          if (propsArray.length > max) {
            return propsArray;
          }

          let value;
          try {
            value = object[name];
          } catch (exc) {
            continue;
          }

          let t = typeof value;
          if (filter(t, value)) {
            propsArray.push(PropRep({
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

      return propsArray;
    },

    render: wrapRender(function () {
      let object = this.props.object;
      let propsArray = this.safePropIterator(object);
      let objectLink = this.props.objectLink || span;

      if (this.props.mode === MODE.TINY || !propsArray.length) {
        return (
          span({className: "objectBox objectBox-object"},
            objectLink({className: "objectTitle"}, this.getTitle(object))
          )
        );
      }

      return (
        span({className: "objectBox objectBox-object"},
          this.getTitle(object),
          objectLink({
            className: "objectLeftBrace",
            object: object
          }, " { "),
          ...propsArray,
          objectLink({
            className: "objectRightBrace",
            object: object
          }, " }")
        )
      );
    }),
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
