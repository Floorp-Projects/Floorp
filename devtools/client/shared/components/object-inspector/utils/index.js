/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const client = require("devtools/client/shared/components/object-inspector/utils/client");
const loadProperties = require("devtools/client/shared/components/object-inspector/utils/load-properties");
const node = require("devtools/client/shared/components/object-inspector/utils/node");
const { nodeIsError, nodeIsPrimitive } = node;
const selection = require("devtools/client/shared/components/object-inspector/utils/selection");

const { MODE } = require("devtools/client/shared/components/reps/reps/constants");
const {
  REPS: { Rep, Grip },
} = require("devtools/client/shared/components/reps/reps/rep");

function shouldRenderRootsInReps(roots, props = {}) {
  if (roots.length !== 1) {
    return false;
  }

  const root = roots[0];
  const name = root && root.name;
  return (
    (name === null || typeof name === "undefined") &&
    (nodeIsPrimitive(root) ||
      (nodeIsError(root) && props?.customFormat === true))
  );
}

function renderRep(item, props) {
  return Rep({
    ...props,
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
