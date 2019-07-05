/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A mixin to forward events to on* methods if defined.
 *
 * @param {string} superclass The class to extend.
 * @returns {class}
 */
export default function HandleEventMixin(superclass) {
  return class HandleEvent extends superclass {
    handleEvent(evt) {
      function capitalize(str) {
        return str.charAt(0).toUpperCase() + str.slice(1);
      }
      if (super.handleEvent) {
        super.handleEvent(evt);
      }
      // Check whether event name is a defined function in object.
      let fn = "on" + capitalize(evt.type);
      if (this[fn] && typeof this[fn] === "function") {
        return this[fn](evt);
      }
      return null;
    }
  };
}
