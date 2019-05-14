/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { fetchChildren } = require("./actions/accessibles");

/**
 * Data provider that is responsible for mapping of an accessibles cache to the
 * data format that is supported by the TreeView component.
 * @param {Map}      accessibles accessibles object cache
 * @param {Function} dispatch    react dispatch function that triggers a redux
 *                               action.
 */

class Provider {
  constructor(accessibles, filtered, dispatch) {
    this.accessibles = accessibles;
    this.filtered = filtered;
    this.dispatch = dispatch;
  }

  /**
   * Get accessible's cached children if available, if not fetch them from
   * backend.
   * @param {Object}  accessible accessible object whose children to get.
   * @returns {Array} arraof of accessible children.
   */
  getChildren(accessible) {
    if (!accessible || !accessible.actorID || accessible.childCount === 0) {
      return [];
    }

    const obj = this.accessibles.get(accessible.actorID);
    if (!obj || !obj.children) {
      return this.dispatch(fetchChildren(accessible));
    }

    return obj.children;
  }

  /**
   * Return a flag indicating if an accessible object has any children.
   * @param {Object}    accessible accessible object whose children to get.
   * @returns {Boolean} idicator of whether accessible object has children.
   */
  hasChildren(accessible) {
    return accessible.childCount > 0;
  }

  /**
   * Get a value for an accessible object. Used to render the second (value)
   * column of the accessible tree. Corresponds to an accesible object name, if
   * available.
   * @param {Object}   accessible accessible object
   * @returns {String} accessible object value.
   */
  getValue(accessible) {
    return accessible.name || "";
  }

  /**
   * Get a label for an accessible object. Used to render the first column of
   * the accessible tree. Corresponds to an accessible object role.
   * @param {Object}   accessible accessible object
   * @returns {String} accessible object label.
   */
  getLabel(accessible) {
    return accessible.role;
  }

  /**
   * Get a unique key for an accessible object. Corresponds to an accessible
   * front's actorID.
   * @param {Object}   accessible accessible object
   * @returns {String} a key for an accessible object.
   */
  getKey(accessible) {
    return accessible.actorID;
  }

  /**
   * Get a type of an accesible object. Corresponds to the type of an accessible
   * front.
   * @param {Object}   accessible accessible object
   * @returns {String} accessible object type
   */
  getType(accessible) {
    return accessible.typeName;
  }

  /**
   * Get the depth of the accesible object in the accessibility tree. When the
   * tree is filtered it is flattened and the level is set to 0. Otherwise use
   * internal TreeView level.
   *
   * @param {Object}   accessible
   *                   accessible object
   * @param {Number}   defaultLevel
   *                   default level provided by the TreeView component.
   *
   * @returns {null|Number}
   *          depth level of the accessible object.
   */
  getLevel(accessible, defaultLevel) {
    return this.filtered ? 0 : defaultLevel;
  }
}

exports.Provider = Provider;
