/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Property } = require("devtools/client/dom/content/reducers/grips");

// Implementation

function DomDecorator() {}

/**
 * Decorator for DOM panel tree component. It's responsible for
 * appending an icon to read only properties.
 */
DomDecorator.prototype = {
  getRowClass(object) {
    if (object instanceof Property) {
      const value = object.value;
      const names = [];

      if (value.enumerable) {
        names.push("enumerable");
      }
      if (value.writable) {
        names.push("writable");
      }
      if (value.configurable) {
        names.push("configurable");
      }

      return names;
    }

    return null;
  },

  /**
   * Return custom React template for specified object. The template
   * might depend on specified column.
   */
  getValueRep(value, colId) {},
};

// Exports from this module
exports.DomDecorator = DomDecorator;
