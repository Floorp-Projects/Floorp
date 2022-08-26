/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  /**
   * Implementation of the default data provider. A provider is state less
   * object responsible for transformation data (usually a state) to
   * a structure that can be directly consumed by the tree-view component.
   */
  const ObjectProvider = {
    getChildren(object) {
      const children = [];

      if (object instanceof ObjectProperty) {
        object = object.value;
      }

      if (!object) {
        return [];
      }

      if (typeof object == "string") {
        return [];
      }

      for (const prop in object) {
        try {
          children.push(new ObjectProperty(prop, object[prop]));
        } catch (e) {
          console.error(e);
        }
      }
      return children;
    },

    hasChildren(object) {
      if (object instanceof ObjectProperty) {
        object = object.value;
      }

      if (!object) {
        return false;
      }

      if (typeof object == "string") {
        return false;
      }

      if (typeof object !== "object") {
        return false;
      }

      return !!Object.keys(object).length;
    },

    getLabel(object) {
      return object instanceof ObjectProperty ? object.name : null;
    },

    getValue(object) {
      return object instanceof ObjectProperty ? object.value : null;
    },

    getKey(object) {
      return object instanceof ObjectProperty ? object.name : null;
    },

    getType(object) {
      return object instanceof ObjectProperty
        ? typeof object.value
        : typeof object;
    },
  };

  function ObjectProperty(name, value) {
    this.name = name;
    this.value = value;
  }

  // Exports from this module
  exports.ObjectProperty = ObjectProperty;
  exports.ObjectProvider = ObjectProvider;
});
