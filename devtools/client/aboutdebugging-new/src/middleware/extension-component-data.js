/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DEBUG_TARGETS, REQUEST_EXTENSIONS_SUCCESS } = require("../constants");

const {
  getExtensionUuid,
  parseFileUri,
} = require("../modules/extensions-helper");

/**
 * This middleware converts extensions object that get from DebuggerClient.listAddons()
 * to data which is used in DebugTargetItem.
 */
const extensionComponentDataMiddleware = store => next => action => {
  switch (action.type) {
    case REQUEST_EXTENSIONS_SUCCESS: {
      action.installedExtensions = toComponentData(action.installedExtensions);
      action.temporaryExtensions = toComponentData(action.temporaryExtensions);
      break;
    }
  }

  return next(action);
};

function getFilePath(extension) {
  // Only show file system paths, and only for temporarily installed add-ons.
  if (
    !extension.temporarilyInstalled ||
    !extension.url ||
    !extension.url.startsWith("file://")
  ) {
    return null;
  }

  return parseFileUri(extension.url);
}

function toComponentData(extensions) {
  return extensions.map(extension => {
    const type = DEBUG_TARGETS.EXTENSION;
    const {
      actor,
      iconDataURL,
      iconURL,
      id,
      manifestURL,
      name,
      warnings,
    } = extension;
    const icon =
      iconDataURL ||
      iconURL ||
      "chrome://mozapps/skin/extensions/extensionGeneric.svg";
    const location = getFilePath(extension);
    const uuid = getExtensionUuid(extension);
    return {
      name,
      icon,
      id,
      type,
      details: {
        actor,
        location,
        manifestURL,
        uuid,
        warnings: warnings || [],
      },
    };
  });
}

module.exports = extensionComponentDataMiddleware;
