/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Decodes an anonymous naming scheme that
// spider monkey implements based on "Naming Anonymous JavaScript Functions"
// http://johnjbarton.github.io/nonymous/index.html
const objectProperty = /([\w\d\$#]+)$/;
const arrayProperty = /\[(.*?)\]$/;
const functionProperty = /([\w\d]+)[\/\.<]*?$/;
const annonymousProperty = /([\w\d]+)\(\^\)$/;
const displayNameScenarios = [
  objectProperty,
  arrayProperty,
  functionProperty,
  annonymousProperty,
];
const includeSpace = /\s/;
export function simplifyDisplayName(displayName) {
  // if the display name has a space it has already been mapped
  if (!displayName || includeSpace.exec(displayName)) {
    return displayName;
  }

  for (const reg of displayNameScenarios) {
    const match = reg.exec(displayName);
    if (match) {
      return match[1];
    }
  }

  return displayName;
}

const displayNameLibraryMap = {
  Babel: {
    tryCatch: "Async",
  },
  Backbone: {
    "extend/child": "Create Class",
    ".create": "Create Model",
  },
  jQuery: {
    "jQuery.event.dispatch": "Dispatch Event",
  },
  React: {
    // eslint-disable-next-line max-len
    "ReactCompositeComponent._renderValidatedComponentWithoutOwnerOrContext/renderedElement<":
      "Render",
    _renderValidatedComponentWithoutOwnerOrContext: "Render",
  },
  VueJS: {
    "renderMixin/Vue.prototype._render": "Render",
  },
  Webpack: {
    // eslint-disable-next-line camelcase
    __webpack_require__: "Bootstrap",
  },
};

/**
 * Compute the typical way to show a frame or function to the user.
 *
 * @param {Object} frameOrFunc
 *        Either a frame or a func object.
 *        Frame object is typically created via create.js::createFrame
 *        Func object comes from ast reducer and getSymbols selector.
 * @param {Boolean} shouldMapDisplayName
 *        True by default, will try to translate internal framework function name
 *        into a most explicit and simplier name.
 * @param {Object} l10n
 *        The localization object.
 */
export function formatDisplayName(
  frameOrFunc,
  { shouldMapDisplayName = true } = {},
  l10n
) {
  // All the following attributes are only available on Frame objects
  const { library, displayName, originalDisplayName } = frameOrFunc;
  let displayedName;

  // If the frame was identified to relate to a library,
  // lookup for pretty name for the most important method of some frameworks
  if (library && shouldMapDisplayName) {
    displayedName = displayNameLibraryMap[library]?.[displayName];
  }

  // Frames for original sources may have both displayName for the generated source,
  // or originalDisplayName for the original source.
  // (in case original and generated have distinct function names in uglified sources)
  //
  // Also fallback to "name" attribute when the passed object is a Func object.
  if (!displayedName) {
    displayedName = originalDisplayName || displayName || frameOrFunc.name;
  }

  if (!displayedName) {
    return l10n.getStr("anonymousFunction");
  }

  return simplifyDisplayName(displayedName);
}

export function formatCopyName(frame, l10n, shouldDisplayOriginalLocation) {
  const displayName = formatDisplayName(frame, undefined, l10n);
  const location = shouldDisplayOriginalLocation
    ? frame.location
    : frame.generatedLocation;
  const fileName = location.source.url || location.source.id;
  const frameLocation = frame.location.line;

  return `${displayName} (${fileName}#${frameLocation})`;
}
