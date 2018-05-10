"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.simplifyDisplayName = simplifyDisplayName;
exports.formatDisplayName = formatDisplayName;
exports.formatCopyName = formatCopyName;

var _source = require("../../source");

var _utils = require("../../../utils/utils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// eslint-disable-next-line max-len
// Decodes an anonymous naming scheme that
// spider monkey implements based on "Naming Anonymous JavaScript Functions"
// http://johnjbarton.github.io/nonymous/index.html
const objectProperty = /([\w\d]+)$/;
const arrayProperty = /\[(.*?)\]$/;
const functionProperty = /([\w\d]+)[\/\.<]*?$/;
const annonymousProperty = /([\w\d]+)\(\^\)$/;

function simplifyDisplayName(displayName) {
  // if the display name has a space it has already been mapped
  if (/\s/.exec(displayName)) {
    return displayName;
  }

  const scenarios = [objectProperty, arrayProperty, functionProperty, annonymousProperty];

  for (const reg of scenarios) {
    const match = reg.exec(displayName);

    if (match) {
      return match[1];
    }
  }

  return displayName;
}

const displayNameMap = {
  Babel: {
    tryCatch: "Async"
  },
  Backbone: {
    "extend/child": "Create Class",
    ".create": "Create Model"
  },
  jQuery: {
    "jQuery.event.dispatch": "Dispatch Event"
  },
  React: {
    // eslint-disable-next-line max-len
    "ReactCompositeComponent._renderValidatedComponentWithoutOwnerOrContext/renderedElement<": "Render",
    _renderValidatedComponentWithoutOwnerOrContext: "Render"
  },
  VueJS: {
    "renderMixin/Vue.prototype._render": "Render"
  },
  Webpack: {
    // eslint-disable-next-line camelcase
    __webpack_require__: "Bootstrap"
  }
};

function mapDisplayNames(frame, library) {
  const {
    displayName
  } = frame;
  return displayNameMap[library] && displayNameMap[library][displayName] || displayName;
}

function formatDisplayName(frame, {
  shouldMapDisplayName = true
} = {}) {
  let {
    displayName,
    originalDisplayName,
    library
  } = frame;
  displayName = originalDisplayName || displayName;

  if (library && shouldMapDisplayName) {
    displayName = mapDisplayNames(frame, library);
  }

  displayName = simplifyDisplayName(displayName);
  return (0, _utils.endTruncateStr)(displayName, 25);
}

function formatCopyName(frame) {
  const displayName = formatDisplayName(frame);
  const fileName = (0, _source.getFilename)(frame.source);
  const frameLocation = frame.location.line;
  return `${displayName} (${fileName}#${frameLocation})`;
}