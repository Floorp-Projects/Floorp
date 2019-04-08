/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import assert from "../../utils/assert";
import { recordEvent } from "../../utils/telemetry";
import { remapBreakpoints } from "../breakpoints";

import { setSymbols } from "./symbols";
import { prettyPrint } from "../../workers/pretty-print";
import { getPrettySourceURL, isLoaded } from "../../utils/source";
import { loadSourceText } from "./loadSourceText";
import { mapFrames } from "../pause";
import { selectSpecificLocation } from "../sources";

import {
  getSource,
  getSourceFromId,
  getSourceByURL,
  getSelectedLocation,
  getThreadContext
} from "../../selectors";

import type { Action, ThunkArgs } from "../types";
import { selectSource } from "./select";
import type { JsSource, Source, Context } from "../../types";

export async function prettyPrintSource(
  sourceMaps: any,
  prettySource: Source,
  generatedSource: any
) {
  const url = getPrettySourceURL(generatedSource.url);
  const { code, mappings } = await prettyPrint({
    source: generatedSource,
    url: url
  });
  await sourceMaps.applySourceMap(generatedSource.id, url, code, mappings);

  // The source map URL service used by other devtools listens to changes to
  // sources based on their actor IDs, so apply the mapping there too.
  for (const sourceActor of generatedSource.actors) {
    await sourceMaps.applySourceMap(sourceActor.actor, url, code, mappings);
  }
  return {
    id: prettySource.id,
    text: code,
    contentType: "text/javascript"
  };
}

export function createPrettySource(cx: Context, sourceId: string) {
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
      introductionType: undefined,
      isExtension: false,
      actors: []
    };

    dispatch(({ type: "ADD_SOURCE", cx, source: prettySource }: Action));
    await dispatch(selectSource(cx, prettySource.id));

    return prettySource;
  };
}

function selectPrettyLocation(cx: Context, prettySource: Source) {
  return async ({ dispatch, sourceMaps, getState }: ThunkArgs) => {
    let location = getSelectedLocation(getState());

    if (location) {
      location = await sourceMaps.getOriginalLocation(location);
      return dispatch(
        selectSpecificLocation(cx, { ...location, sourceId: prettySource.id })
      );
    }

    return dispatch(selectSource(cx, prettySource.id));
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
export function togglePrettyPrint(cx: Context, sourceId: string) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const source = getSource(getState(), sourceId);
    if (!source) {
      return {};
    }

    if (!source.isPrettyPrinted) {
      recordEvent("pretty_print");
    }

    if (!isLoaded(source)) {
      await dispatch(loadSourceText({ cx, source }));
    }

    assert(
      sourceMaps.isGeneratedId(sourceId),
      "Pretty-printing only allowed on generated sources"
    );

    const url = getPrettySourceURL(source.url);
    const prettySource = getSourceByURL(getState(), url);

    if (prettySource) {
      return dispatch(selectPrettyLocation(cx, prettySource));
    }

    const newPrettySource = await dispatch(createPrettySource(cx, sourceId));
    await dispatch(selectPrettyLocation(cx, newPrettySource));

    const threadcx = getThreadContext(getState());
    await dispatch(mapFrames(threadcx));

    await dispatch(setSymbols({ cx, source: newPrettySource }));

    await dispatch(remapBreakpoints(cx, sourceId));

    return newPrettySource;
  };
}
