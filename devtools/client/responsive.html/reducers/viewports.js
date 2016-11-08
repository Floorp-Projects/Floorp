/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_VIEWPORT_PIXEL_RATIO,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT,
} = require("../actions/index");

let nextViewportId = 0;

const INITIAL_VIEWPORTS = [];
const INITIAL_VIEWPORT = {
  id: nextViewportId++,
  device: "",
  width: 320,
  height: 480,
  pixelRatio: 0,
};

let reducers = {

  [ADD_VIEWPORT](viewports) {
    // For the moment, there can be at most one viewport.
    if (viewports.length === 1) {
      return viewports;
    }
    return [...viewports, Object.assign({}, INITIAL_VIEWPORT)];
  },

  [CHANGE_DEVICE](viewports, { id, device }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      return Object.assign({}, viewport, {
        device,
      });
    });
  },

  [CHANGE_VIEWPORT_PIXEL_RATIO](viewports, {id, pixelRatio }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      return Object.assign({}, viewport, {
        pixelRatio,
      });
    });
  },

  [RESIZE_VIEWPORT](viewports, { id, width, height }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      if (!width) {
        width = viewport.width;
      }
      if (!height) {
        height = viewport.height;
      }

      return Object.assign({}, viewport, {
        width,
        height,
      });
    });
  },

  [ROTATE_VIEWPORT](viewports, { id }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      return Object.assign({}, viewport, {
        width: viewport.height,
        height: viewport.width,
      });
    });
  },

};

module.exports = function (viewports = INITIAL_VIEWPORTS, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return viewports;
  }
  return reducer(viewports, action);
};
