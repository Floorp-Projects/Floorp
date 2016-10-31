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
  const { Caption } = createFactories(require("./caption"));
  const { PropRep } = createFactories(require("./prop-rep"));
  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders generic grip. Grip is client representation
   * of remote JS object and is used as an input object
   * for this rep component.
   */
  const GripRep = React.createClass({
    displayName: "Grip",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      mode: React.PropTypes.string,
      isInterestingProp: React.PropTypes.func
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
      if (Object.keys(object.preview).includes("wrappedValue")) {
        const { Rep } = createFactories(require("./rep"));

        return [Rep({
          object: object.preview.wrappedValue,
          mode: this.props.mode || "tiny",
          defaultRep: Grip,
        })];
      }

      // Property filter. Show only interesting properties to the user.
      let isInterestingProp = this.props.isInterestingProp || ((type, value) => {
        return (
          type == "boolean" ||
          type == "number" ||
          (type == "string" && value.length != 0)
        );
      });

      let ownProperties = object.preview ? object.preview.ownProperties : {};
      let propertiesLength = object.preview && object.preview.ownPropertiesLength
        ? object.preview.ownPropertiesLength
        : object.ownPropertyLength;
      let indexes = this.getPropIndexes(ownProperties, max, isInterestingProp);
      if (indexes.length < max && indexes.length < propertiesLength) {
        // There are not enough props yet. Then add uninteresting props to display them.
        indexes = indexes.concat(
          this.getPropIndexes(ownProperties, max - indexes.length, (t, value, name) => {
            return !isInterestingProp(t, value, name);
          })
        );
      }

      const truncate = Object.keys(ownProperties).length > max;
      let props = this.getProps(ownProperties, indexes, truncate);
      if (truncate) {
        // There are some undisplayed props. Then display "more...".
        let objectLink = this.props.objectLink || span;

        props.push(Caption({
          object: objectLink({
            object: object
          }, `${object.ownPropertyLength - max} more…`)
        }));
      }

      return props;
    },

    /**
     * Get props ordered by index.
     *
     * @param {Object} ownProperties Props object.
     * @param {Array} indexes Indexes of props.
     * @param {Boolean} truncate true if the grip will be truncated.
     * @return {Array} Props.
     */
    getProps: function (ownProperties, indexes, truncate) {
      let props = [];

      // Make indexes ordered by ascending.
      indexes.sort(function (a, b) {
        return a - b;
      });

      indexes.forEach((i) => {
        let name = Object.keys(ownProperties)[i];
        let prop = ownProperties[name];
        let value = prop.value !== undefined ? prop.value : prop;
        props.push(PropRep(Object.assign({}, this.props, {
          mode: "tiny",
          name: name,
          object: value,
          equal: ": ",
          delim: i !== indexes.length - 1 || truncate ? ", " : "",
          defaultRep: Grip
        })));
      });

      return props;
    },

    /**
     * Get the indexes of props in the object.
     *
     * @param {Object} ownProperties Props object.
     * @param {Number} max The maximum length of indexes array.
     * @param {Function} filter Filter the props you want.
     * @return {Array} Indexes of interesting props in the object.
     */
    getPropIndexes: function (ownProperties, max, filter) {
      let indexes = [];

      try {
        let i = 0;
        for (let name in ownProperties) {
          if (indexes.length >= max) {
            return indexes;
          }

          let prop = ownProperties[name];
          let value = prop.value !== undefined ? prop.value : prop;

          // Type is specified in grip's "class" field and for primitive
          // values use typeof.
          let type = (value.class || typeof value);
          type = type.toLowerCase();

          if (filter(type, value, name)) {
            indexes.push(i);
          }
          i++;
        }
      } catch (err) {
        console.error(err);
      }

      return indexes;
    },

    render: function () {
      let object = this.props.object;
      let props = this.safePropIterator(object,
        (this.props.mode == "long") ? 100 : 3);

      let objectLink = this.props.objectLink || span;
      if (this.props.mode == "tiny") {
        return (
          span({className: "objectBox objectBox-object"},
            this.getTitle(object),
            objectLink({
              className: "objectLeftBrace",
              object: object
            }, "")
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
          ...props,
          objectLink({
            className: "objectRightBrace",
            object: object
          }, " }")
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

  let Grip = {
    rep: GripRep,
    supportsObject: supportsObject
  };

  // Exports from this module
  exports.Grip = Grip;
});
