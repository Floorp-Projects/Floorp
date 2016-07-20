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
  // Dependencies
  const { createFactories, isGrip } = require("./rep-utils");
  const { ObjectBox } = createFactories(require("./object-box"));
  const { Caption } = createFactories(require("./caption"));
  const { PropRep } = createFactories(require("./prop-rep"));
  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders generic grip. Grip is client representation
   * of remote JS object and is used as an input object
   * for this rep component.
   */
  const Grip = React.createClass({
    displayName: "Grip",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
    },

    getTitle: function (object) {
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: object
        }, object.class);
      }
      return object.class || "Object";
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
      // Property filter. Show only interesting properties to the user.
      let isInterestingProp = (type, value) => {
        return (
          type == "boolean" ||
          type == "number" ||
          type == "string" ||
          type == "object"
        );
      };

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

      // getProps() can return max+1 properties (it can't return more)
      // to indicate that there is more props than allowed. Remove the last
      // one and append 'more…' postfix in such case.
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
        // NOTE: do not change comp._store.props directly to update a property,
        // it should be re-rendered or cloned with changed props
        let last = props.length - 1;
        props[last] = React.cloneElement(props[last], {
          delim: ""
        });
      }

      return props;
    },

    getProps: function (object, max, filter) {
      let props = [];

      max = max || 3;
      if (!object) {
        return props;
      }

      try {
        let ownProperties = object.preview ? object.preview.ownProperties : [];
        for (let name in ownProperties) {
          if (props.length > max) {
            return props;
          }

          let prop = ownProperties[name];
          let value = prop.value || {};

          // Type is specified in grip's "class" field and for primitive
          // values use typeof.
          let type = (value.class || typeof value);
          type = type.toLowerCase();

          // Show only interesting properties.
          if (filter(type, value)) {
            props.push(PropRep(Object.assign({}, this.props, {
              key: name,
              mode: "tiny",
              name: name,
              object: value,
              equal: ": ",
              delim: ", ",
            })));
          }
        }
      } catch (err) {
        console.error(err);
      }

      return props;
    },

    render: function () {
      let object = this.props.object;
      let props = this.safePropIterator(object,
        (this.props.mode == "long") ? 100 : 3);

      let objectLink = this.props.objectLink || span;
      if (this.props.mode == "tiny" || !props.length) {
        return (
          ObjectBox({className: "object"},
            this.getTitle(object),
            objectLink({
              className: "objectLeftBrace",
              role: "presentation",
              object: object
            }, "")
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
          }, " {"),
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

  // Registration
  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return (object.preview && object.preview.ownProperties);
  }

  // Exports from this module
  exports.Grip = {
    rep: Grip,
    supportsObject: supportsObject
  };
});
