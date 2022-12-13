/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PROMISE } from "../utils/middleware/promise";
import {
  getSource,
  getSourceTextContent,
  getSettledSourceTextContent,
  getGeneratedSource,
  getSourcesEpoch,
  getBreakpointsForSource,
  getSourceActorsForSource,
  getFirstSourceActorForGeneratedSource,
} from "../../selectors";
import { addBreakpoint } from "../breakpoints";

import { prettyPrintSource } from "./prettyPrint";
import { isFulfilled, fulfilled } from "../../utils/async-value";

import { isPretty } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { memoizeableAction } from "../../utils/memoizableAction";

async function loadGeneratedSource(state, sourceActor, client) {
  // If no source actor can be found then the text for the
  // source cannot be loaded.
  if (!sourceActor) {
    throw new Error("Source actor is null or not defined");
  }

  let response;
  try {
    response = await client.sourceContents(sourceActor);
  } catch (e) {
    throw new Error(`sourceContents failed: ${e}`);
  }

  return {
    text: response.source,
    contentType: response.contentType || "text/javascript",
  };
}

async function loadOriginalSource(state, source, client, sourceMaps) {
  if (isPretty(source)) {
    const generatedSource = getGeneratedSource(state, source);
    if (!generatedSource) {
      throw new Error("Unable to find minified original.");
    }

    const content = getSettledSourceTextContent(
      state,
      createLocation({
        sourceId: generatedSource.id,
      })
    );

    return prettyPrintSource(
      sourceMaps,
      generatedSource,
      content,
      getSourceActorsForSource(state, generatedSource.id)
    );
  }

  const result = await sourceMaps.getOriginalSourceText(source.id);
  if (!result) {
    // The way we currently try to load and select a pending
    // selected location, it is possible that we will try to fetch the
    // original source text right after the source map has been cleared
    // after a navigation event.
    throw new Error("Original source text unavailable");
  }
  return result;
}

async function loadGeneratedSourceTextPromise(
  cx,
  sourceActor,
  { dispatch, getState, client, parser }
) {
  const epoch = getSourcesEpoch(getState());

  await dispatch({
    type: "LOAD_GENERATED_SOURCE_TEXT",
    sourceActorId: sourceActor.actor,
    epoch,
    [PROMISE]: loadGeneratedSource(getState(), sourceActor, client),
  });

  await setParserAndBreakpointsTextContent(
    cx,
    sourceActor.source,
    sourceActor.actor,
    {
      state: getState(),
      parser,
      dispatch,
    }
  );
}

async function loadOriginalSourceTextPromise(
  cx,
  source,
  { dispatch, getState, client, sourceMaps, parser }
) {
  const epoch = getSourcesEpoch(getState());

  await dispatch({
    type: "LOAD_ORIGINAL_SOURCE_TEXT",
    sourceId: source.id,
    epoch,
    [PROMISE]: loadOriginalSource(getState(), source, client, sourceMaps),
  });

  await setParserAndBreakpointsTextContent(cx, source.id, null, {
    state: getState(),
    parser,
    dispatch,
  });
}

async function setParserAndBreakpointsTextContent(
  cx,
  sourceId,
  sourceActorId,
  { dispatch, state, parser }
) {
  const source = getSource(state, sourceId);

  if (!source) {
    return;
  }

  const content = getSettledSourceTextContent(
    state,
    createLocation({
      sourceId: source.id,
      sourceActorId,
    })
  );

  if (!source.isWasm && content) {
    parser.setSource(
      source.id,
      isFulfilled(content)
        ? content.value
        : { type: "text", value: "", contentType: undefined }
    );

    // Update the text in any breakpoints for this source by re-adding them.
    const breakpoints = getBreakpointsForSource(state, source.id);
    for (const { location, options, disabled } of breakpoints) {
      await dispatch(addBreakpoint(cx, location, options, disabled));
    }
  }
}

/**
 * Loads the source text for the generated source based of the source actor
 * @param {Object} sourceActor
 *                 There can be more than one source actor per source
 *                 so the source actor needs to be specified. This is
 *                 required for generated sources but will be null for
 *                 original/pretty printed sources.
 */
export const loadGeneratedSourceText = memoizeableAction(
  "loadGeneratedSourceText",
  {
    getValue: ({ sourceActor }, { getState }) => {
      if (!sourceActor) {
        return null;
      }

      const sourceTextContent = getSourceTextContent(
        getState(),
        createLocation({
          sourceId: sourceActor.source,
          sourceActorId: sourceActor.actor,
        })
      );

      if (!sourceTextContent || sourceTextContent.state === "pending") {
        return sourceTextContent;
      }

      // This currently swallows source-load-failure since we return fulfilled
      // here when content.state === "rejected". In an ideal world we should
      // propagate that error upward.
      return fulfilled(sourceTextContent);
    },
    createKey: ({ sourceActor }, { getState }) => {
      const epoch = getSourcesEpoch(getState());
      return `${epoch}:${sourceActor.actor}`;
    },
    action: ({ cx, sourceActor }, thunkArgs) =>
      loadGeneratedSourceTextPromise(cx, sourceActor, thunkArgs),
  }
);

/**
 * Loads the source text for an original source and source actor
 * @param {Object} source
 *                 The original source to load the source text
 */
export const loadOriginalSourceText = memoizeableAction(
  "loadOriginalSourceText",
  {
    getValue: ({ source }, { getState }) => {
      if (!source) {
        return null;
      }

      const sourceTextContent = getSourceTextContent(
        getState(),
        createLocation({
          sourceId: source.id,
        })
      );
      if (!sourceTextContent || sourceTextContent.state === "pending") {
        return sourceTextContent;
      }

      // This currently swallows source-load-failure since we return fulfilled
      // here when content.state === "rejected". In an ideal world we should
      // propagate that error upward.
      return fulfilled(sourceTextContent);
    },
    createKey: ({ source }, { getState }) => {
      const epoch = getSourcesEpoch(getState());
      return `${epoch}:${source.id}`;
    },
    action: ({ cx, source }, thunkArgs) =>
      loadOriginalSourceTextPromise(cx, source, thunkArgs),
  }
);

export function loadSourceText(cx, source, sourceActor) {
  return async ({ dispatch, getState }) => {
    if (!source) {
      return null;
    }
    if (source.isOriginal) {
      return dispatch(loadOriginalSourceText({ cx, source }));
    }
    if (!sourceActor) {
      sourceActor = getFirstSourceActorForGeneratedSource(
        getState(),
        source.id,
        source.thread
      );
    }
    return dispatch(loadGeneratedSourceText({ cx, sourceActor }));
  };
}
