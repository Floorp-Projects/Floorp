/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_GEOMETRY_EDITOR_ENABLED,
  UPDATE_LAYOUT,
  UPDATE_OFFSET_PARENT,
} = require("../actions/index");

const INITIAL_BOX_MODEL = {
  geometryEditorEnabled: false,
  layout: {},
  offsetParent: null
};

const reducers = {

  [UPDATE_GEOMETRY_EDITOR_ENABLED](boxModel, { enabled }) {
    return Object.assign({}, boxModel, {
      geometryEditorEnabled: enabled,
    });
  },

  [UPDATE_LAYOUT](boxModel, { layout }) {
    return Object.assign({}, boxModel, {
      layout,
    });
  },

  [UPDATE_OFFSET_PARENT](boxModel, { offsetParent }) {
    return Object.assign({}, boxModel, {
      offsetParent,
    });
  },

};

module.exports = function(boxModel = INITIAL_BOX_MODEL, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return boxModel;
  }
  return reducer(boxModel, action);
};
