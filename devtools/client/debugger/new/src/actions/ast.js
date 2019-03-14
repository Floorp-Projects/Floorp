/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSource,
  getSourceFromId,
  getSourceThreads,
  getSymbols,
  getSelectedLocation
} from "../selectors";

import { mapFrames } from "./pause";
import { updateTab } from "./tabs";

import { PROMISE } from "./utils/middleware/promise";

import { setInScopeLines } from "./ast/setInScopeLines";

import * as parser from "../workers/parser";

import { isLoaded } from "../utils/source";

import type { SourceId } from "../types";
import type { ThunkArgs, Action } from "./types";

export function setSourceMetaData(sourceId: SourceId) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source || !isLoaded(source) || source.isWasm) {
      return;
    }

    const framework = await parser.getFramework(source.id);
    if (framework) {
      dispatch(updateTab(source, framework));
    }

    dispatch(
      ({
        type: "SET_SOURCE_METADATA",
        sourceId: source.id,
        sourceMetaData: {
          framework
        }
      }: Action)
    );
  };
}

export function setSymbols(sourceId: SourceId) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);

    if (source.isWasm || getSymbols(getState(), source) || !isLoaded(source)) {
      return;
    }

    await dispatch({
      type: "SET_SYMBOLS",
      sourceId,
      [PROMISE]: parser.getSymbols(sourceId)
    });

    const threads = getSourceThreads(getState(), source);
    await Promise.all(threads.map(thread => dispatch(mapFrames(thread))));

    await dispatch(setSourceMetaData(sourceId));
  };
}

export function setOutOfScopeLocations() {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    const source = getSourceFromId(getState(), location.sourceId);

    if (!isLoaded(source)) {
      return;
    }

    let locations = null;
    if (location.line && source && !source.isWasm) {
      locations = await parser.findOutOfScopeLocations(
        source.id,
        ((location: any): parser.AstPosition)
      );
    }

    dispatch(
      ({
        type: "OUT_OF_SCOPE_LOCATIONS",
        locations
      }: Action)
    );
    dispatch(setInScopeLines());
  };
}
