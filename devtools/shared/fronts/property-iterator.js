/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  propertyIteratorSpec,
} = require("devtools/shared/specs/property-iterator");

/**
 * A PropertyIteratorFront provides a way to access to property names and
 * values of an object efficiently, slice by slice.
 * Note that the properties can be sorted in the backend,
 * this is controled while creating the PropertyIteratorFront
 * from ObjectClient.enumProperties.
 */
class PropertyIteratorFront extends FrontClassWithSpec(propertyIteratorSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;
  }

  get actor() {
    return this._grip.actor;
  }

  /**
   * Get the total number of properties available in the iterator.
   */
  get count() {
    return this._grip.count;
  }

  /**
   * Get one or more property names that correspond to the positions in the
   * indexes parameter.
   *
   * @param indexes Array
   *        An array of property indexes.
   */
  names(indexes) {
    return super.names({ indexes });
  }

  /**
   * Get a set of following property value(s).
   *
   * @param start Number
   *        The index of the first property to fetch.
   * @param count Number
   *        The number of properties to fetch.
   */
  slice(start, count) {
    return super.slice({ start, count });
  }

  form(form) {
    this._grip = form;
  }
}

exports.PropertyIteratorFront = PropertyIteratorFront;
registerFront(PropertyIteratorFront);
