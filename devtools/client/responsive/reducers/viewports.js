/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_PIXEL_RATIO,
  CHANGE_VIEWPORT_ANGLE,
  EDIT_DEVICE,
  REMOVE_DEVICE_ASSOCIATION,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT,
} = require("../actions/index");

const VIEWPORT_WIDTH_PREF = "devtools.responsive.viewport.width";
const VIEWPORT_HEIGHT_PREF = "devtools.responsive.viewport.height";
const VIEWPORT_PIXEL_RATIO_PREF = "devtools.responsive.viewport.pixelRatio";
const VIEWPORT_ANGLE_PREF = "devtools.responsive.viewport.angle";

let nextViewportId = 0;

const INITIAL_VIEWPORTS = [];
const INITIAL_VIEWPORT = {
  id: nextViewportId++,
  angle: Services.prefs.getIntPref(VIEWPORT_ANGLE_PREF, 0),
  device: "",
  deviceType: "",
  height: Services.prefs.getIntPref(VIEWPORT_HEIGHT_PREF, 480),
  width: Services.prefs.getIntPref(VIEWPORT_WIDTH_PREF, 320),
  pixelRatio: Services.prefs.getIntPref(VIEWPORT_PIXEL_RATIO_PREF, 0),
  userContextId: 0,
};

const reducers = {
  [ADD_VIEWPORT](viewports, { userContextId }) {
    // For the moment, there can be at most one viewport.
    if (viewports.length === 1) {
      return viewports;
    }

    return [
      ...viewports,
      {
        ...INITIAL_VIEWPORT,
        userContextId,
      },
    ];
  },

  [CHANGE_DEVICE](viewports, { id, device, deviceType }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      return {
        ...viewport,
        device,
        deviceType,
      };
    });
  },

  [CHANGE_PIXEL_RATIO](viewports, { id, pixelRatio }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      Services.prefs.setIntPref(VIEWPORT_PIXEL_RATIO_PREF, pixelRatio);

      return {
        ...viewport,
        pixelRatio,
      };
    });
  },

  [CHANGE_VIEWPORT_ANGLE](viewports, { id, angle }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      Services.prefs.setIntPref(VIEWPORT_ANGLE_PREF, angle);

      return {
        ...viewport,
        angle,
      };
    });
  },

  [EDIT_DEVICE](viewports, { viewport, newDevice, deviceType }) {
    if (!viewport) {
      return viewports;
    }

    return viewports.map(v => {
      if (v.id !== viewport.id) {
        return viewport;
      }

      Services.prefs.setIntPref(VIEWPORT_WIDTH_PREF, newDevice.width);
      Services.prefs.setIntPref(VIEWPORT_HEIGHT_PREF, newDevice.height);
      Services.prefs.setIntPref(
        VIEWPORT_PIXEL_RATIO_PREF,
        newDevice.pixelRatio
      );

      return {
        ...viewport,
        device: newDevice.name,
        deviceType,
        height: newDevice.height,
        width: newDevice.width,
        pixelRatio: newDevice.pixelRatio,
        userAgent: newDevice.userAgent,
        touch: newDevice.touch,
      };
    });
  },

  [REMOVE_DEVICE_ASSOCIATION](viewports, { id }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      return {
        ...viewport,
        device: "",
        deviceType: "",
      };
    });
  },

  [RESIZE_VIEWPORT](viewports, { id, width, height }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      if (!height) {
        height = viewport.height;
      }

      if (!width) {
        width = viewport.width;
      }

      Services.prefs.setIntPref(VIEWPORT_WIDTH_PREF, width);
      Services.prefs.setIntPref(VIEWPORT_HEIGHT_PREF, height);

      return {
        ...viewport,
        height,
        width,
      };
    });
  },

  [ROTATE_VIEWPORT](viewports, { id }) {
    return viewports.map(viewport => {
      if (viewport.id !== id) {
        return viewport;
      }

      const height = viewport.width;
      const width = viewport.height;

      Services.prefs.setIntPref(VIEWPORT_WIDTH_PREF, width);
      Services.prefs.setIntPref(VIEWPORT_HEIGHT_PREF, height);

      return {
        ...viewport,
        height,
        width,
      };
    });
  },
};

module.exports = function(viewports = INITIAL_VIEWPORTS, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return viewports;
  }
  return reducer(viewports, action);
};
