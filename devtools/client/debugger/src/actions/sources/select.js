/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for the sources state
 * @module actions/sources
 */

import { isOriginalId } from "devtools-source-map";

import { getSourceFromId, getSourceWithContent } from "../../reducers/sources";
import { getSourcesForTabs } from "../../reducers/tabs";
import { setOutOfScopeLocations } from "../ast";
import { setSymbols } from "./symbols";
import { closeActiveSearch, updateActiveFileSearch } from "../ui";
import { isFulfilled } from "../../utils/async-value";
import { togglePrettyPrint } from "./prettyPrint";
import { addTab, closeTab } from "../tabs";
import { loadSourceText } from "./loadSourceText";

import { prefs } from "../../utils/prefs";
import { shouldPrettyPrint, isMinified } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { mapLocation } from "../../utils/source-maps";

import {
  getSource,
  getSourceByURL,
  getPrettySource,
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
} from "../../selectors";

import type {
  SourceLocation,
  PartialPosition,
  Source,
  Context,
} from "../../types";
import type { ThunkArgs } from "../types";

export const setSelectedLocation = (
  cx: Context,
  source: Source,
  location: SourceLocation
) => ({
  type: "SET_SELECTED_LOCATION",
  cx,
  source,
  location,
});

export const setPendingSelectedLocation = (
  cx: Context,
  url: string,
  options: Object
) => ({
  type: "SET_PENDING_SELECTED_LOCATION",
  cx,
  url: url,
  line: options.location ? options.location.line : null,
});

export const clearSelectedLocation = (cx: Context) => ({
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
 *
 * @memberof actions/sources
 * @static
 */
export function selectSourceURL(
  cx: Context,
  url: string,
  options: PartialPosition = { line: 1 }
) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
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
 * @memberof actions/sources
 * @static
 */
export function selectSource(
  cx: Context,
  sourceId: string,
  options: PartialPosition = { line: 1 }
) {
  return async ({ dispatch }: ThunkArgs) => {
    const location = createLocation({ ...options, sourceId });
    return dispatch(selectSpecificLocation(cx, location));
  };
}

/**
 * @memberof actions/sources
 * @static
 */
export function selectLocation(
  cx: Context,
  location: SourceLocation,
  { keepContext = true }: Object = {}
) {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    const currentSource = getSelectedSource(getState());

    if (!client) {
      // No connection, do nothing. This happens when the debugger is
      // shut down too fast and it tries to display a default source.
      return;
    }

    let source = getSource(getState(), location.sourceId);
    if (!source) {
      // If there is no source we deselect the current selected source
      return dispatch(clearSelectedLocation(cx));
    }

    const activeSearch = getActiveSearch(getState());
    if (activeSearch && activeSearch !== "file") {
      dispatch(closeActiveSearch());
    }

    // Preserve the current source map context (original / generated)
    // when navigting to a new location.
    const selectedSource = getSelectedSource(getState());
    if (
      keepContext &&
      selectedSource &&
      isOriginalId(selectedSource.id) != isOriginalId(location.sourceId)
    ) {
      location = await mapLocation(getState(), sourceMaps, location);
      source = getSourceFromId(getState(), location.sourceId);
    }

    const tabSources = getSourcesForTabs(getState());
    if (!tabSources.includes(source)) {
      dispatch(addTab(source));
    }

    dispatch(setSelectedLocation(cx, source, location));

    await dispatch(loadSourceText({ cx, source }));
    const loadedSource = getSource(getState(), source.id);

    if (!loadedSource) {
      // If there was a navigation while we were loading the loadedSource
      return;
    }
    const sourceWithContent = getSourceWithContent(getState(), source.id);
    const sourceContent =
      sourceWithContent.content && isFulfilled(sourceWithContent.content)
        ? sourceWithContent.content.value
        : null;

    if (
      keepContext &&
      prefs.autoPrettyPrint &&
      !getPrettySource(getState(), loadedSource.id) &&
      shouldPrettyPrint(
        loadedSource,
        sourceContent || { type: "text", value: "", contentType: undefined }
      ) &&
      isMinified(sourceWithContent)
    ) {
      await dispatch(togglePrettyPrint(cx, loadedSource.id));
      dispatch(closeTab(cx, loadedSource));
    }

    dispatch(setSymbols({ cx, source: loadedSource }));
    dispatch(setOutOfScopeLocations(cx));

    // If a new source is selected update the file search results
    const newSource = getSelectedSource(getState());
    if (currentSource && currentSource !== newSource) {
      dispatch(updateActiveFileSearch(cx));
    }
  };
}

/**
 * @memberof actions/sources
 * @static
 */
export function selectSpecificLocation(cx: Context, location: SourceLocation) {
  return selectLocation(cx, location, { keepContext: false });
}

/**
 * @memberof actions/sources
 * @static
 */
export function jumpToMappedLocation(cx: Context, location: SourceLocation) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    if (!client) {
      return;
    }

    const pairedLocation = await mapLocation(getState(), sourceMaps, location);

    return dispatch(selectSpecificLocation(cx, { ...pairedLocation }));
  };
}

export function jumpToMappedSelectedLocation(cx: Context) {
  return async function({ dispatch, getState }: ThunkArgs) {
    const location = getSelectedLocation(getState());
    if (!location) {
      return;
    }

    await dispatch(jumpToMappedLocation(cx, location));
  };
}
