/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Sources content reducer.
 *
 * This store the textual content for each source.
 */

import { pending, fulfilled, rejected } from "../utils/async-value";

export function initialSourcesContentState() {
  return {
    /**
     * Text content of all the original sources.
     * This is large data, so this is only fetched on-demand for a subset of sources.
     * This state attribute is mutable in order to avoid cloning this possibly large map
     * on each new source. But selectors are never based on the map. Instead they only
     * query elements of the map.
     *
     * Map(source id => AsyncValue<String>)
     */
    mutableOriginalSourceTextContentMapBySourceId: new Map(),

    /**
     * Text content of all the generated sources.
     *
     * Map(source actor is => AsyncValue<String>)
     */
    mutableGeneratedSourceTextContentMapBySourceActorId: new Map(),

    /**
     * Incremental number that is bumped each time we navigate to a new page.
     *
     * This is used to better handle async race condition where we mix previous page data
     * with the new page. As sources are keyed by URL we may easily conflate the two page loads data.
     */
    epoch: 1,
  };
}

function update(state = initialSourcesContentState(), action) {
  switch (action.type) {
    case "LOAD_ORIGINAL_SOURCE_TEXT":
      if (!action.source) {
        throw new Error("Missing source");
      }
      return updateSourceTextContent(state, action);

    case "LOAD_GENERATED_SOURCE_TEXT":
      if (!action.sourceActor) {
        throw new Error("Missing source actor.");
      }
      return updateSourceTextContent(state, action);

    case "REMOVE_THREAD":
      return removeThread(state, action);
  }

  return state;
}

/*
 * Update a source's loaded text content.
 */
function updateSourceTextContent(state, action) {
  // If there was a navigation between the time the action was started and
  // completed, we don't want to update the store.
  if (action.epoch !== state.epoch) {
    return state;
  }

  let content;
  if (action.status === "start") {
    content = pending();
  } else if (action.status === "error") {
    content = rejected(action.error);
  } else if (typeof action.value.text === "string") {
    content = fulfilled({
      type: "text",
      value: action.value.text,
      contentType: action.value.contentType,
    });
  } else {
    content = fulfilled({
      type: "wasm",
      value: action.value.text,
    });
  }

  if (action.source && action.sourceActor) {
    throw new Error(
      "Both the source and the source actor should not exist at the same time"
    );
  }

  if (action.source) {
    state.mutableOriginalSourceTextContentMapBySourceId.set(
      action.source.id,
      content
    );
  }

  if (action.sourceActor) {
    state.mutableGeneratedSourceTextContentMapBySourceActorId.set(
      action.sourceActor.id,
      content
    );
  }

  return {
    ...state,
  };
}

function removeThread(state, action) {
  const originalSizeBefore =
    state.mutableOriginalSourceTextContentMapBySourceId.size;
  for (const source of action.sources) {
    state.mutableOriginalSourceTextContentMapBySourceId.delete(source.id);
  }
  const generatedSizeBefore =
    state.mutableGeneratedSourceTextContentMapBySourceActorId.size;
  for (const actor of action.actors) {
    state.mutableGeneratedSourceTextContentMapBySourceActorId.delete(actor.id);
  }
  if (
    originalSizeBefore !=
      state.mutableOriginalSourceTextContentMapBySourceId.size ||
    generatedSizeBefore !=
      state.mutableGeneratedSourceTextContentMapBySourceActorId.size
  ) {
    return { ...state };
  }
  return state;
}

export default update;
