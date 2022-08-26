/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  fetchProperties,
} = require("devtools/client/dom/content/actions/grips");
const { Property } = require("devtools/client/dom/content/reducers/grips");

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
  getChildren(object) {
    let grip = object;
    if (object instanceof Property) {
      grip = this.getValue(object);
    }

    if (!grip || !grip.actorID) {
      return [];
    }

    const props = this.grips.get(grip.actorID);
    if (!props) {
      // Fetch missing data from the backend. Returning a promise
      // from data provider causes the tree to show a spinner.
      return this.dispatch(fetchProperties(grip));
    }

    return props;
  },

  hasChildren(object) {
    if (object instanceof Property) {
      const value = this.getValue(object);
      if (!value) {
        return false;
      }
      const grip = value?.getGrip ? value.getGrip() : value;

      let hasChildren = grip.ownPropertyLength > 0;

      if (grip.preview) {
        hasChildren = hasChildren || grip.preview.ownPropertiesLength > 0;
      }

      if (grip.preview) {
        const preview = grip.preview;
        const k = preview.kind;
        const objectsWithProps = ["DOMNode", "ObjectWithURL"];
        hasChildren = hasChildren || objectsWithProps.includes(k);
        hasChildren = hasChildren || (k == "ArrayLike" && !!preview.length);
      }

      return grip.type == "object" && hasChildren;
    }

    return null;
  },

  getValue(object) {
    if (object instanceof Property) {
      const value = object.value;
      return typeof value.value != "undefined"
        ? value.value
        : value.getterValue;
    }

    return object;
  },

  getLabel(object) {
    return object instanceof Property ? object.name : null;
  },

  getKey(object) {
    return object instanceof Property ? object.key : null;
  },

  getType(object) {
    const grip = object?.getGrip ? object.getGrip() : object;
    return grip.class ? grip.class : "";
  },
};

// Exports from this module
exports.GripProvider = GripProvider;
