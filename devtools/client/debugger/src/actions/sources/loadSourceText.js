/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PROMISE } from "../utils/middleware/promise";
import {
  getSourceTextContent,
  getSettledSourceTextContent,
  getGeneratedSource,
  getSourcesEpoch,
  getBreakpointsForSource,
  getSourceActorsForSource,
  getFirstSourceActorForGeneratedSource,
} from "../../selectors/index";
import { addBreakpoint } from "../breakpoints/index";

import { prettyPrintSourceTextContent } from "./prettyPrint";
import { isFulfilled, fulfilled } from "../../utils/async-value";

import { isPretty } from "../../utils/source";
import { createLocation } from "../../utils/location";
import { memoizeableAction } from "../../utils/memoizableAction";

async function loadGeneratedSource(sourceActor, { client }) {
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

async function loadOriginalSource(
  source,
  { getState, client, sourceMapLoader, prettyPrintWorker }
) {
  if (isPretty(source)) {
    const generatedSource = getGeneratedSource(getState(), source);
    if (!generatedSource) {
      throw new Error("Unable to find minified original.");
    }

    const content = getSettledSourceTextContent(
      getState(),
      createLocation({
        source: generatedSource,
      })
    );

    return prettyPrintSourceTextContent(
      sourceMapLoader,
      prettyPrintWorker,
      generatedSource,
      content,
      getSourceActorsForSource(getState(), generatedSource.id)
    );
  }

  const result = await sourceMapLoader.getOriginalSourceText(source.id);
  if (!result) {
    // The way we currently try to load and select a pending
    // selected location, it is possible that we will try to fetch the
    // original source text right after the source map has been cleared
    // after a navigation event.
    throw new Error("Original source text unavailable");
  }
  return result;
}

async function loadGeneratedSourceTextPromise(sourceActor, thunkArgs) {
  const { dispatch, getState } = thunkArgs;
  const epoch = getSourcesEpoch(getState());

  await dispatch({
    type: "LOAD_GENERATED_SOURCE_TEXT",
    sourceActor,
    epoch,
    [PROMISE]: loadGeneratedSource(sourceActor, thunkArgs),
  });

  await onSourceTextContentAvailable(
    sourceActor.sourceObject,
    sourceActor,
    thunkArgs
  );
}

async function loadOriginalSourceTextPromise(source, thunkArgs) {
  const { dispatch, getState } = thunkArgs;
  const epoch = getSourcesEpoch(getState());
  await dispatch({
    type: "LOAD_ORIGINAL_SOURCE_TEXT",
    source,
    epoch,
    [PROMISE]: loadOriginalSource(source, thunkArgs),
  });

  await onSourceTextContentAvailable(source, null, thunkArgs);
}

/**
 * Function called everytime a new original or generated source gets its text content
 * fetched from the server and registered in the reducer.
 *
 * @param {Object} source
 * @param {Object} sourceActor (optional)
 *        If this is a generated source, we expect a precise source actor.
 * @param {Object} thunkArgs
 */
async function onSourceTextContentAvailable(
  source,
  sourceActor,
  { dispatch, getState, parserWorker }
) {
  const location = createLocation({
    source,
    sourceActor,
  });
  const content = getSettledSourceTextContent(getState(), location);
  if (!content) {
    return;
  }

  if (parserWorker.isLocationSupported(location)) {
    parserWorker.setSource(
      source.id,
      isFulfilled(content)
        ? content.value
        : { type: "text", value: "", contentType: undefined }
    );
  }

  // Update the text in any breakpoints for this source by re-adding them.
  const breakpoints = getBreakpointsForSource(getState(), source);
  for (const breakpoint of breakpoints) {
    await dispatch(
      addBreakpoint(
        breakpoint.location,
        breakpoint.options,
        breakpoint.disabled
      )
    );
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
    getValue: (sourceActor, { getState }) => {
      if (!sourceActor) {
        return null;
      }

      const sourceTextContent = getSourceTextContent(
        getState(),
        createLocation({
          source: sourceActor.sourceObject,
          sourceActor,
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
    createKey: (sourceActor, { getState }) => {
      const epoch = getSourcesEpoch(getState());
      return `${epoch}:${sourceActor.actor}`;
    },
    action: (sourceActor, thunkArgs) =>
      loadGeneratedSourceTextPromise(sourceActor, thunkArgs),
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
    getValue: (source, { getState }) => {
      if (!source) {
        return null;
      }

      const sourceTextContent = getSourceTextContent(
        getState(),
        createLocation({
          source,
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
    createKey: (source, { getState }) => {
      const epoch = getSourcesEpoch(getState());
      return `${epoch}:${source.id}`;
    },
    action: (source, thunkArgs) =>
      loadOriginalSourceTextPromise(source, thunkArgs),
  }
);

export function loadSourceText(source, sourceActor) {
  return async ({ dispatch, getState }) => {
    if (!source) {
      return null;
    }
    if (source.isOriginal) {
      return dispatch(loadOriginalSourceText(source));
    }
    if (!sourceActor) {
      sourceActor = getFirstSourceActorForGeneratedSource(
        getState(),
        source.id
      );
    }
    return dispatch(loadGeneratedSourceText(sourceActor));
  };
}
