/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSource,
  getSourceFromId,
  hasSymbols,
  getSelectedLocation,
  isPaused
} from "../selectors";

import { mapFrames, fetchExtra } from "./pause";
import { updateTab } from "./tabs";

import { PROMISE } from "./utils/middleware/promise";

import { setInScopeLines } from "./ast/setInScopeLines";
import { setPausePoints } from "./ast/setPausePoints";
export { setPausePoints };

import {
  getSymbols,
  findOutOfScopeLocations,
  getFramework,
  type AstPosition
} from "../workers/parser";

import { isLoaded } from "../utils/source";

import type { SourceId } from "../types";
import type { ThunkArgs, Action } from "./types";

export function setSourceMetaData(sourceId: SourceId) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source || !isLoaded(source) || source.isWasm) {
      return;
    }

    const framework = await getFramework(source.id);
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

    if (source.isWasm || hasSymbols(getState(), source) || !isLoaded(source)) {
      return;
    }

    await dispatch({
      type: "SET_SYMBOLS",
      sourceId,
      [PROMISE]: getSymbols(sourceId)
    });

    if (isPaused(getState())) {
      await dispatch(fetchExtra());
      await dispatch(mapFrames());
    }

    await dispatch(setPausePoints(sourceId));
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

    let locations = null;
    if (location.line && source && !source.isWasm && isPaused(getState())) {
      locations = await findOutOfScopeLocations(
        source.id,
        ((location: any): AstPosition)
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
