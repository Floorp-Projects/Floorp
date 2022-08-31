/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { isOriginalId } from "devtools-source-map";

import { setSymbols } from "./symbols";
import { setInScopeLines } from "../ast";
import { closeActiveSearch, updateActiveFileSearch } from "../ui";
import { togglePrettyPrint } from "./prettyPrint";
import { addTab, closeTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";
import { mapDisplayNames } from "../pause";
import { setBreakableLines } from ".";

import { prefs } from "../../utils/prefs";
import { isMinified } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { getRelatedMapLocation } from "../../utils/source-maps";

import {
  getSource,
  getSourceByURL,
  getPrettySource,
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
  canPrettyPrintSource,
  getIsCurrentThreadPaused,
  getLocationSource,
  getSourceTextContent,
  tabExists,
} from "../../selectors";

// This is only used by jest tests (and within this module)
export const setSelectedLocation = (cx, source, location) => ({
  type: "SET_SELECTED_LOCATION",
  cx,
  source,
  location,
});

// This is only used by jest tests (and within this module)
export const setPendingSelectedLocation = (cx, url, options) => ({
  type: "SET_PENDING_SELECTED_LOCATION",
  cx,
  url,
  line: options?.line,
  column: options?.column,
});

// This is only used by jest tests (and within this module)
export const clearSelectedLocation = cx => ({
  type: "CLEAR_SELECTED_LOCATION",
  cx,
});

/**
 * Deterministically select a source that has a given URL. This will
 * work regardless of the connection status or if the source exists
 * yet.
 *
 * This exists mostly for external things to interact with the
 * debugger.
 */
export function selectSourceURL(cx, url, options) {
  return async ({ dispatch, getState, sourceMaps }) => {
    const source = getSourceByURL(getState(), url);
    if (!source) {
      return dispatch(setPendingSelectedLocation(cx, url, options));
    }

    const sourceId = source.id;
    const location = createLocation({ ...options, sourceId });
    return dispatch(selectLocation(cx, location));
  };
}

/**
 * Wrapper around selectLocation, which creates the location object for us.
 * Note that it ignores the currently selected source and will select
 * the precise generated/original source passed as argument.
 *
 * @param {Object} cx
 * @param {String} sourceId
 *        The precise source to select.
 * @param {Object} location
 *        Optional precise location to select, if we need to select
 *        a precise line/column.
 */
export function selectSource(cx, sourceId, location = {}) {
  return async ({ dispatch }) => {
    location = createLocation({ ...location, sourceId });
    return dispatch(selectSpecificLocation(cx, location));
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
 * @param {Object} cx
 * @param {Object} location
 * @param {Object} options
 * @param {boolean} options.keepContext
 *        If false, this will ignore the currently selected source
 *        and select the generated or original location, even if we
 *        were currently selecting the other source type.
 */
export function selectLocation(cx, location, { keepContext = true } = {}) {
  return async ({ dispatch, getState, sourceMaps, client }) => {
    const currentSource = getSelectedSource(getState());

    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    let source = getLocationSource(getState(), location);

    if (!source) {
      // If there is no source we deselect the current selected source
      dispatch(clearSelectedLocation(cx));
      return;
    }

    const activeSearch = getActiveSearch(getState());
    if (activeSearch && activeSearch !== "file") {
      dispatch(closeActiveSearch());
    }

    // Preserve the current source map context (original / generated)
    // when navigating to a new location.
    // i.e. if keepContext isn't manually overriden to false,
    // we will convert the source we want to select to either
    // original/generated in order to match the currently selected one.
    // If the currently selected source is original, we will
    // automatically map `location` to refer to the original source,
    // even if that used to refer only to the generated source.
    const selectedSource = getSelectedSource(getState());
    if (
      keepContext &&
      selectedSource &&
      selectedSource.isOriginal != isOriginalId(location.sourceId)
    ) {
      // getRelatedMapLocation will just convert to the related generated/original location.
      // i.e if the original location is passed, the related generated location will be returned and vice versa.
      location = await getRelatedMapLocation(getState(), sourceMaps, location);
      source = getLocationSource(getState(), location);
    }

    if (!tabExists(getState(), source.id)) {
      dispatch(addTab(source));
    }

    dispatch(setSelectedLocation(cx, source, location));

    await dispatch(loadSourceText({ cx, source }));
    await dispatch(setBreakableLines(cx, source.id));

    const loadedSource = getSource(getState(), source.id);

    if (!loadedSource) {
      // If there was a navigation while we were loading the loadedSource
      return;
    }

    const sourceTextContent = getSourceTextContent(getState(), source.id);

    if (
      keepContext &&
      prefs.autoPrettyPrint &&
      !getPrettySource(getState(), loadedSource.id) &&
      canPrettyPrintSource(getState(), loadedSource.id) &&
      isMinified(source, sourceTextContent)
    ) {
      await dispatch(togglePrettyPrint(cx, loadedSource.id));
      dispatch(closeTab(cx, loadedSource));
    }

    await dispatch(setSymbols({ cx, source: loadedSource }));
    dispatch(setInScopeLines(cx));

    if (getIsCurrentThreadPaused(getState())) {
      await dispatch(mapDisplayNames(cx));
    }

    // If a new source is selected update the file search results
    const newSource = getSelectedSource(getState());
    if (currentSource && currentSource !== newSource) {
      dispatch(updateActiveFileSearch(cx));
    }
  };
}

/**
 * Select a location while ignoring the currently selected source.
 * This will select the generated location even if the currently
 * select source is an original source. And the other way around.
 *
 * @param {Object} cx
 * @param {Object} location
 *        The location to select, object which includes enough
 *        information to specify a precise source, line and column.
 */
export function selectSpecificLocation(cx, location) {
  return selectLocation(cx, location, { keepContext: false });
}

/**
 * Select the "mapped location".
 *
 * If the passed location is on a generated source, select the
 * related location in the original source.
 * If the passed location is on an original source, select the
 * related location in the generated source.
 */
export function jumpToMappedLocation(cx, location) {
  return async function({ dispatch, getState, client, sourceMaps }) {
    if (!client) {
      return null;
    }

    // Map to either an original or a generated source location
    const pairedLocation = await getRelatedMapLocation(
      getState(),
      sourceMaps,
      location
    );

    return dispatch(selectSpecificLocation(cx, pairedLocation));
  };
}

// This is only used by tests
export function jumpToMappedSelectedLocation(cx) {
  return async function({ dispatch, getState }) {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    await dispatch(jumpToMappedLocation(cx, location));
  };
}
