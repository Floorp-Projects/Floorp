/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSymbols, getSource, getSelectedFrame, getCurrentThread } from ".";
import { findClosestClass } from "../utils/ast";

import type { State } from "../reducers/types";

export function inComponent(state: State) {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);
  if (!selectedFrame) {
    return;
  }

  const source = getSource(state, selectedFrame.location.sourceId);
  if (!source) {
    return;
  }

  const symbols = getSymbols(state, source);

  if (!symbols || symbols.loading) {
    return;
  }

  const closestClass = findClosestClass(symbols, selectedFrame.location);
  if (!closestClass) {
    return null;
  }

  const inReactFile = symbols.framework == "React";
  const { parent } = closestClass;
  const isComponent = parent && parent.name.includes("Component");

  if (inReactFile && isComponent) {
    return closestClass.name;
  }
}
