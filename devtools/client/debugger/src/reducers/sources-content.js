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
     * Text content of all the sources.
     * This is large data, so this is only fetched on-demand for a subset of sources.
     * This state attribute is mutable in order to avoid cloning this possibly large map
     * on each new source. But selectors are never based on the map. Instead they only
     * query elements of the map.
     *
     * Map(source id => AsyncValue<String>)
     */
    mutableTextContentMap: new Map(),

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
    case "LOAD_SOURCE_TEXT":
      return updateSourceTextContent(state, action);

    case "NAVIGATE":
      return {
        ...initialSourcesContentState(),
        epoch: state.epoch + 1,
      };
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
      actorId: action.value.actorId,
      type: "text",
      value: action.value.text,
      contentType: action.value.contentType,
    });
  } else {
    content = fulfilled({
      actorId: action.value.actorId,
      type: "wasm",
      value: action.value.text,
    });
  }

  state.mutableTextContentMap.set(action.sourceId, content);

  return {
    ...state,
  };
}

export default update;
