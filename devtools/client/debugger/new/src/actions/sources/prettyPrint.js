/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { remapBreakpoints, setBreakpointPositions } from "../breakpoints";

import { setSymbols } from "../ast";
import { prettyPrint } from "../../workers/pretty-print";
import { setSource } from "../../workers/parser";
import { getPrettySourceURL, isLoaded } from "../../utils/source";
import { loadSourceText } from "./loadSourceText";
import { mapFrames } from "../pause";
import { selectSpecificLocation } from "../sources";

import {
  getSource,
  getSourceFromId,
  getSourceThreads,
  getSourceByURL,
  getSelectedLocation
} from "../../selectors";

import type { Action, ThunkArgs } from "../types";
import { selectSource } from "./select";
import type { JsSource } from "../../types";

export function createPrettySource(sourceId: string) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const source = getSourceFromId(getState(), sourceId);
    const url = getPrettySourceURL(source.url);
    const id = await sourceMaps.generatedToOriginalId(sourceId, url);

    const prettySource: JsSource = {
      url,
      relativeUrl: url,
      id,
      isBlackBoxed: false,
      isPrettyPrinted: true,
      isWasm: false,
      contentType: "text/javascript",
      loadedState: "loading",
      introductionUrl: null,
      isExtension: false,
      actors: []
    };

    dispatch(({ type: "ADD_SOURCE", source: prettySource }: Action));
    dispatch(selectSource(prettySource.id));

    const { code, mappings } = await prettyPrint({ source, url });
    await sourceMaps.applySourceMap(source.id, url, code, mappings);

    // The source map URL service used by other devtools listens to changes to
    // sources based on their actor IDs, so apply the mapping there too.
    for (const sourceActor of source.actors) {
      await sourceMaps.applySourceMap(sourceActor.actor, url, code, mappings);
    }

    const loadedPrettySource: JsSource = {
      ...prettySource,
      text: code,
      loadedState: "loaded"
    };

    setSource(loadedPrettySource);
    dispatch(({ type: "UPDATE_SOURCE", source: loadedPrettySource }: Action));
    await dispatch(setBreakpointPositions(loadedPrettySource.id));

    return prettySource;
  };
}

/**
 * Toggle the pretty printing of a source's text. All subsequent calls to
 * |getText| will return the pretty-toggled text. Nothing will happen for
 * non-javascript files.
 *
 * @memberof actions/sources
 * @static
 * @param string id The source form from the RDP.
 * @returns Promise
 *          A promise that resolves to [aSource, prettyText] or rejects to
 *          [aSource, error].
 */
export function togglePrettyPrint(sourceId: string) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return {};
    }

    if (!source.isPrettyPrinted) {
      recordEvent("pretty_print");
    }

    if (!isLoaded(source)) {
      await dispatch(loadSourceText(source));
    }

    assert(
      sourceMaps.isGeneratedId(sourceId),
      "Pretty-printing only allowed on generated sources"
    );

    const selectedLocation = getSelectedLocation(getState());
    const url = getPrettySourceURL(source.url);
    const prettySource = getSourceByURL(getState(), url);

    const options = {};
    if (selectedLocation) {
      options.location = await sourceMaps.getOriginalLocation(selectedLocation);
    }

    if (prettySource) {
      const _sourceId = prettySource.id;
      return dispatch(
        selectSpecificLocation({ ...options.location, sourceId: _sourceId })
      );
    }

    const newPrettySource = await dispatch(createPrettySource(sourceId));

    await dispatch(remapBreakpoints(sourceId));

    const threads = getSourceThreads(getState(), source);
    await Promise.all(threads.map(thread => dispatch(mapFrames(thread))));

    await dispatch(setSymbols(newPrettySource.id));

    dispatch(
      selectSpecificLocation({
        ...options.location,
        sourceId: newPrettySource.id
      })
    );

    return newPrettySource;
  };
}
