/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { setSymbols } from "./symbols";
import { setInScopeLines } from "../ast";
import { togglePrettyPrint } from "./prettyPrint";
import { addTab, closeTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";
import { setBreakableLines } from ".";

import { prefs } from "../../utils/prefs";
import { isMinified } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { getRelatedMapLocation } from "../../utils/source-maps";

import {
  getSource,
  getFirstSourceActorForGeneratedSource,
  getSourceByURL,
  getPrettySource,
  getSelectedLocation,
  getShouldSelectOriginalLocation,
  canPrettyPrintSource,
  getSourceTextContent,
  tabExists,
} from "../../selectors";

// This is only used by jest tests (and within this module)
export const setSelectedLocation = (
  location,
  shouldSelectOriginalLocation
) => ({
  type: "SET_SELECTED_LOCATION",
  location,
  shouldSelectOriginalLocation,
});

// This is only used by jest tests (and within this module)
export const setPendingSelectedLocation = (url, options) => ({
  type: "SET_PENDING_SELECTED_LOCATION",
  url,
  line: options?.line,
  column: options?.column,
});

// This is only used by jest tests (and within this module)
export const clearSelectedLocation = () => ({
  type: "CLEAR_SELECTED_LOCATION",
});

/**
 * Deterministically select a source that has a given URL. This will
 * work regardless of the connection status or if the source exists
 * yet.
 *
 * This exists mostly for external things to interact with the
 * debugger.
 */
export function selectSourceURL(url, options) {
  return async ({ dispatch, getState }) => {
    const source = getSourceByURL(getState(), url);
    if (!source) {
      return dispatch(setPendingSelectedLocation(url, options));
    }

    const location = createLocation({ ...options, source });
    return dispatch(selectLocation(location));
  };
}

/**
 * Wrapper around selectLocation, which creates the location object for us.
 * Note that it ignores the currently selected source and will select
 * the precise generated/original source passed as argument.
 *
 * @param {String} source
 *        The precise source to select.
 * @param {String} sourceActor
 *        The specific source actor of the source to
 *        select the source text. This is optional.
 */
export function selectSource(source, sourceActor) {
  return async ({ dispatch }) => {
    // `createLocation` requires a source object, but we may use selectSource to close the last tab,
    // where source will be null and the location will be an empty object.
    const location = source ? createLocation({ source, sourceActor }) : {};

    return dispatch(selectSpecificLocation(location));
  };
}

/**
 * Select a new location.
 * This will automatically select the source in the source tree (if visible)
 * and open the source (a new tab and the source editor)
 * as well as highlight a precise line in the editor.
 *
 * Note that by default, this may map your passed location to the original
 * or generated location based on the selected source state. (see keepContext)
 *
 * @param {Object} location
 * @param {Object} options
 * @param {boolean} options.keepContext
 *        If false, this will ignore the currently selected source
 *        and select the generated or original location, even if we
 *        were currently selecting the other source type.
 */
export function selectLocation(location, { keepContext = true } = {}) {
  return async thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;

    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    let source = location.source;

    if (!source) {
      // If there is no source we deselect the current selected source
      dispatch(clearSelectedLocation());
      return;
    }

    // Preserve the current source map context (original / generated)
    // when navigating to a new location.
    // i.e. if keepContext isn't manually overriden to false,
    // we will convert the source we want to select to either
    // original/generated in order to match the currently selected one.
    // If the currently selected source is original, we will
    // automatically map `location` to refer to the original source,
    // even if that used to refer only to the generated source.
    let shouldSelectOriginalLocation = getShouldSelectOriginalLocation(
      getState()
    );
    if (keepContext) {
      if (shouldSelectOriginalLocation != location.source.isOriginal) {
        // getRelatedMapLocation will convert to the related generated/original location.
        // i.e if the original location is passed, the related generated location will be returned and vice versa.
        location = await getRelatedMapLocation(location, thunkArgs);
        // Note that getRelatedMapLocation may return the exact same location.
        // For example, if the source-map is half broken, it may return a generated location
        // while we were selecting original locations. So we may be seeing bundles intermittently
        // when stepping through broken source maps. And we will see original sources when stepping
        // through functional original sources.

        source = location.source;
      }
    } else {
      shouldSelectOriginalLocation = location.source.isOriginal;
    }

    let sourceActor = location.sourceActor;
    if (!sourceActor) {
      sourceActor = getFirstSourceActorForGeneratedSource(
        getState(),
        source.id
      );
      location = createLocation({ ...location, sourceActor });
    }

    if (!tabExists(getState(), source.id)) {
      dispatch(addTab(source, sourceActor));
    }

    dispatch(setSelectedLocation(location, shouldSelectOriginalLocation));

    await dispatch(loadSourceText(source, sourceActor));

    await dispatch(setBreakableLines(location));

    const loadedSource = getSource(getState(), source.id);

    if (!loadedSource) {
      // If there was a navigation while we were loading the loadedSource
      return;
    }

    const sourceTextContent = getSourceTextContent(getState(), location);

    if (
      keepContext &&
      prefs.autoPrettyPrint &&
      !getPrettySource(getState(), loadedSource.id) &&
      canPrettyPrintSource(getState(), location) &&
      isMinified(source, sourceTextContent)
    ) {
      await dispatch(togglePrettyPrint(loadedSource.id));
      dispatch(closeTab(loadedSource));
    }

    await dispatch(setSymbols(location));
    // /!\ we don't historicaly wait for this async action
    dispatch(setInScopeLines());
  };
}

/**
 * Select a location while ignoring the currently selected source.
 * This will select the generated location even if the currently
 * select source is an original source. And the other way around.
 *
 * @param {Object} location
 *        The location to select, object which includes enough
 *        information to specify a precise source, line and column.
 */
export function selectSpecificLocation(location) {
  return selectLocation(location, { keepContext: false });
}

/**
 * Select the "mapped location".
 *
 * If the passed location is on a generated source, select the
 * related location in the original source.
 * If the passed location is on an original source, select the
 * related location in the generated source.
 */
export function jumpToMappedLocation(location) {
  return async function (thunkArgs) {
    const { client, dispatch } = thunkArgs;
    if (!client) {
      return null;
    }

    // Map to either an original or a generated source location
    const pairedLocation = await getRelatedMapLocation(location, thunkArgs);

    return dispatch(selectSpecificLocation(pairedLocation));
  };
}

// This is only used by tests
export function jumpToMappedSelectedLocation() {
  return async function ({ dispatch, getState }) {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    await dispatch(jumpToMappedLocation(location));
  };
}
