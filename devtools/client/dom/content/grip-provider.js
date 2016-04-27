/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { fetchProperties } = require("./actions/grips");
const { Property } = require("./reducers/grips");

// Implementation
function GripProvider(grips, dispatch) {
  this.grips = grips;
  this.dispatch = dispatch;
}

/**
 * This object provides data for the tree displayed in the tooltip
 * content.
 */
GripProvider.prototype = {
  /**
   * Fetches properties from the backend. These properties might be
   * displayed as child objects in e.g. a tree UI widget.
   */
  getChildren: function (object) {
    let grip = object;
    if (object instanceof Property) {
      grip = this.getValue(object);
    }

    if (!grip || !grip.actor) {
      return [];
    }

    let props = this.grips.get(grip.actor);
    if (!props) {
      // Fetch missing data from the backend. Returning a promise
      // from data provider causes the tree to show a spinner.
      return this.dispatch(fetchProperties(grip));
    }

    return props;
  },

  hasChildren: function (object) {
    if (object instanceof Property) {
      let value = this.getValue(object);
      if (!value) {
        return false;
      }

      let hasChildren = value.ownPropertyLength > 0;

      if (value.preview) {
        hasChildren = hasChildren || value.preview.ownPropertiesLength > 0;
      }

      if (value.preview) {
        let preview = value.preview;
        let k = preview.kind;
        let objectsWithProps = ["DOMNode", "ObjectWithURL"];
        hasChildren = hasChildren || (objectsWithProps.indexOf(k) != -1);
        hasChildren = hasChildren || (k == "ArrayLike" && preview.length > 0);
      }

      return (value.type == "object" && hasChildren);
    }

    return null;
  },

  getValue: function (object) {
    if (object instanceof Property) {
      let value = object.value;
      return (typeof value.value != "undefined") ? value.value :
        value.getterValue;
    }

    return object;
  },

  getLabel: function (object) {
    return (object instanceof Property) ? object.name : null;
  },

  getKey: function (object) {
    return (object instanceof Property) ? object.key : null;
  },

  getType: function (object) {
    return object.class ? object.class : "";
  },
};

// Exports from this module
exports.GripProvider = GripProvider;
