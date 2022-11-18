/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const client = require("resource://devtools/client/shared/components/object-inspector/utils/client.js");
const loadProperties = require("resource://devtools/client/shared/components/object-inspector/utils/load-properties.js");
const node = require("resource://devtools/client/shared/components/object-inspector/utils/node.js");
const { nodeIsError, nodeIsPrimitive } = node;
const selection = require("resource://devtools/client/shared/components/object-inspector/utils/selection.js");

const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const {
  REPS: { Rep, Grip },
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

function shouldRenderRootsInReps(roots, props = {}) {
  if (roots.length !== 1) {
    return false;
  }

  const root = roots[0];
  const name = root && root.name;

  return (
    (name === null || typeof name === "undefined") &&
    (nodeIsPrimitive(root) ||
      (root?.contents?.value?.useCustomFormatter === true &&
        Array.isArray(root?.contents?.value?.header)) ||
      (nodeIsError(root) && props?.customFormat === true))
  );
}

function renderRep(item, props) {
  return Rep({
    ...props,
    front: item.contents.front,
    object: node.getValue(item),
    mode: props.mode || MODE.TINY,
    defaultRep: Grip,
  });
}

module.exports = {
  client,
  loadProperties,
  node,
  renderRep,
  selection,
  shouldRenderRootsInReps,
};
