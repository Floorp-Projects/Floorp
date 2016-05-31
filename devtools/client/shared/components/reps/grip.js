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

  // Shortcuts
  const { span } = React.DOM;

  /**
   * @template TODO docs
   */
  const Grip = React.createClass({
    displayName: "Grip",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
    },

    getTitle: function () {
      return this.props.object.class || "Object";
    },

    longPropIterator: function (object) {
      try {
        return this.propIterator(object, 100);
      } catch (err) {
        console.error(err);
      }
      return [];
    },

    shortPropIterator: function (object) {
      try {
        return this.propIterator(object, 3);
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
        // be able to know whether we should print "more..." or not).
        // Let's display also empty members and functions.
        props = props.concat(this.getProps(object, max, (t, value) => {
          return !isInterestingProp(t, value);
        }));
      }

      // getProps() can return max+1 properties (it can't return more)
      // to indicate that there is more props than allowed. Remove the last
      // one and append 'more...' postfix in such case.
      if (props.length > max) {
        props.pop();
        props.push(Caption({
          key: "more",
          object: "more...",
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
      let props = this.shortPropIterator(object);

      if (this.props.mode == "tiny" || !props.length) {
        return (
          ObjectBox({className: "object"},
            span({className: "objectTitle"}, this.getTitle(object))
          )
        );
      }

      return (
        ObjectBox({className: "object"},
          span({className: "objectTitle"}, this.getTitle(object)),
          span({className: "objectLeftBrace", role: "presentation"}, " {"),
          props,
          span({className: "objectRightBrace"}, "}")
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
      name: React.PropTypes.string,
      equal: React.PropTypes.string,
      delim: React.PropTypes.string,
    },

    render: function () {
      let { Rep } = createFactories(require("./rep"));

      return (
        span({},
          span({
            "className": "nodeName"},
            this.props.name),
          span({
            "className": "objectEqual",
            role: "presentation"},
            this.props.equal
          ),
          Rep(this.props),
          span({
            "className": "objectComma",
            role: "presentation"},
            this.props.delim
          )
        )
      );
    }
  }));

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
