/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const client = require("./client");
const loadProperties = require("./load-properties");
const node = require("./node");
const { nodeIsError, nodeIsPrimitive } = node;
const selection = require("./selection");

const { MODE } = require("../../reps/constants");
const {
  REPS: { Rep, Grip },
} = require("../../reps/rep");
import type { Node, Props } from "../types";

function shouldRenderRootsInReps(roots: Array<Node>): boolean {
  if (roots.length > 1) {
    return false;
  }

  const root = roots[0];
  const name = root && root.name;
  return (
    (name === null || typeof name === "undefined") &&
    (nodeIsPrimitive(root) || nodeIsError(root))
  );
}

function renderRep(item: Node, props: Props) {
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
