/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getValidatedResource, getResourceValues } from "./core";

export function hasResource(state, id) {
  return !!getValidatedResource(state, id);
}

export function getResourceIds(state) {
  return Object.keys(getResourceValues(state));
}

export function getResource(state, id) {
  const validatedState = getValidatedResource(state, id);
  if (!validatedState) {
    throw new Error(`Resource ${id} does not exist`);
  }
  return validatedState.values[id];
}

export function getMappedResource(state, id, map) {
  const validatedState = getValidatedResource(state, id);
  if (!validatedState) {
    throw new Error(`Resource ${id} does not exist`);
  }

  return map(validatedState.values[id], validatedState.identity[id]);
}
