/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";

import { getPrettySourceURL, isPretty, isJavaScript } from "../utils/source";

import { findPosition } from "../utils/breakpoint/breakpointPositions";
import { isFulfilled } from "../utils/async-value";

import { originalToGeneratedId } from "devtools/client/shared/source-map-loader/index";
import { prefs } from "../utils/prefs";

import {
  hasSourceActor,
  getSourceActor,
  getBreakableLinesForSourceActors,
  isSourceActorWithSourceMap,
} from "./source-actors";
import { getSourceTextContent } from "./sources-content";

export function hasSource(state, id) {
  return state.sources.mutableSources.has(id);
}

export function getSource(state, id) {
  return state.sources.mutableSources.get(id);
}

export function getSourceFromId(state, id) {
  const source = getSource(state, id);
  if (!source) {
    console.warn(`source ${id} does not exist`);
  }
  return source;
}

export function getSourceByActorId(state, actorId) {
  if (!hasSourceActor(state, actorId)) {
    return null;
  }

  return getSource(state, getSourceActor(state, actorId).source);
}

function getSourcesByURL(state, url) {
  return state.sources.mutableSourcesPerUrl.get(url) || [];
}

export function getSourceByURL(state, url) {
  const foundSources = getSourcesByURL(state, url);
  return foundSources[0];
}

// This is used by tabs selectors
export function getSpecificSourceByURL(state, url, isOriginal) {
  const foundSources = getSourcesByURL(state, url);
  return foundSources.find(source => source.isOriginal == isOriginal);
}

function getOriginalSourceByURL(state, url) {
  return getSpecificSourceByURL(state, url, true);
}

export function getGeneratedSourceByURL(state, url) {
  return getSpecificSourceByURL(state, url, false);
}

export function getGeneratedSource(state, source) {
  if (!source) {
    return null;
  }

  if (!source.isOriginal) {
    return source;
  }

  return getSourceFromId(state, originalToGeneratedId(source.id));
}

export function getPendingSelectedLocation(state) {
  return state.sources.pendingSelectedLocation;
}

export function getPrettySource(state, id) {
  if (!id) {
    return null;
  }

  const source = getSource(state, id);
  if (!source) {
    return null;
  }

  return getOriginalSourceByURL(state, getPrettySourceURL(source.url));
}

// This is only used by Project Search and tests.
export function getSourceList(state) {
  return [...state.sources.mutableSources.values()];
}

// This is only used by tests and create.js
export function getSourceCount(state) {
  return state.sources.mutableSources.size;
}

export function getSelectedLocation(state) {
  return state.sources.selectedLocation;
}

export const getSelectedSource = createSelector(
  getSelectedLocation,
  selectedLocation => {
    if (!selectedLocation) {
      return undefined;
    }

    return selectedLocation.source;
  }
);

// This is used by tests and pause reducers
export function getSelectedSourceId(state) {
  const source = getSelectedSource(state);
  return source?.id;
}

export function getShouldSelectOriginalLocation(state) {
  return state.sources.shouldSelectOriginalLocation;
}

/**
 * Gets the first source actor for the source and/or thread
 * provided.
 *
 * @param {Object} state
 * @param {String} sourceId
 *         The source used
 * @param {String} [threadId]
 *         The thread to check, this is optional.
 * @param {Object} sourceActor
 *
 */
export function getFirstSourceActorForGeneratedSource(
  state,
  sourceId,
  threadId
) {
  let source = getSource(state, sourceId);
  // The source may have been removed if we are being called by async code
  if (!source) {
    return null;
  }
  if (source.isOriginal) {
    source = getSource(state, originalToGeneratedId(source.id));
  }
  const actors = getSourceActorsForSource(state, source.id);
  if (threadId) {
    return actors.find(actorInfo => actorInfo.thread == threadId) || null;
  }
  return actors[0] || null;
}

/**
 * Get the source actor of the source
 *
 * @param {Object} state
 * @param {String} id
 *        The source id
 * @return {Array<Object>}
 *         List of source actors
 */
export function getSourceActorsForSource(state, id) {
  return state.sources.mutableSourceActors.get(id) || [];
}

export function isSourceWithMap(state, id) {
  const actors = getSourceActorsForSource(state, id);
  return actors.some(actor => isSourceActorWithSourceMap(state, actor.id));
}

export function canPrettyPrintSource(state, location) {
  const sourceId = location.source.id;
  const source = getSource(state, sourceId);
  if (
    !source ||
    isPretty(source) ||
    source.isOriginal ||
    (prefs.clientSourceMapsEnabled && isSourceWithMap(state, sourceId))
  ) {
    return false;
  }

  const content = getSourceTextContent(state, location);
  const sourceContent = content && isFulfilled(content) ? content.value : null;

  if (
    !sourceContent ||
    (!isJavaScript(source, sourceContent) && !source.isHTML)
  ) {
    return false;
  }

  return true;
}

export function getPrettyPrintMessage(state, location) {
  const source = location.source;
  if (!source) {
    return L10N.getStr("sourceTabs.prettyPrint");
  }

  if (isPretty(source)) {
    return L10N.getStr("sourceFooter.prettyPrint.isPrettyPrintedMessage");
  }

  if (source.isOriginal) {
    return L10N.getStr("sourceFooter.prettyPrint.isOriginalMessage");
  }

  if (prefs.clientSourceMapsEnabled && isSourceWithMap(state, source.id)) {
    return L10N.getStr("sourceFooter.prettyPrint.hasSourceMapMessage");
  }

  const content = getSourceTextContent(state, location);

  const sourceContent = content && isFulfilled(content) ? content.value : null;
  if (!sourceContent) {
    return L10N.getStr("sourceFooter.prettyPrint.noContentMessage");
  }

  if (!isJavaScript(source, sourceContent) && !source.isHTML) {
    return L10N.getStr("sourceFooter.prettyPrint.isNotJavascriptMessage");
  }

  return L10N.getStr("sourceTabs.prettyPrint");
}

export function getBreakpointPositionsForSource(state, sourceId) {
  return state.sources.mutableBreakpointPositions.get(sourceId);
}

// This is only used by one test
export function hasBreakpointPositions(state, sourceId) {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function getBreakpointPositionsForLine(state, sourceId, line) {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return positions?.[line];
}

export function getBreakpointPositionsForLocation(state, location) {
  const sourceId = location.source.id;
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return findPosition(positions, location);
}

export function getBreakableLines(state, sourceId) {
  if (!sourceId) {
    return null;
  }
  const source = getSource(state, sourceId);
  if (!source) {
    return null;
  }

  if (source.isOriginal) {
    return state.sources.mutableOriginalBreakableLines.get(sourceId);
  }

  const sourceActors = getSourceActorsForSource(state, sourceId);
  if (!sourceActors.length) {
    return null;
  }

  // We pull generated file breakable lines directly from the source actors
  // so that breakable lines can be added as new source actors on HTML loads.
  return getBreakableLinesForSourceActors(state, sourceActors, source.isHTML);
}

export const getSelectedBreakableLines = createSelector(
  state => {
    const sourceId = getSelectedSourceId(state);
    return sourceId && getBreakableLines(state, sourceId);
  },
  breakableLines => new Set(breakableLines || [])
);

export function isSourceOverridden(state, source) {
  if (!source || !source.url) {
    return false;
  }
  return state.sources.mutableOverrideSources.has(source.url);
}

/**
 * Compute the list of source actors and source objects to be removed
 * when removing a given target/thread.
 *
 * @param {String} threadActorID
 *        The thread to be removed.
 * @return {Object}
 *         An object with two arrays:
 *         - actors: list of source actor objects to remove
 *         - sources: list of source objects to remove
 */
export function getSourcesToRemoveForThread(state, threadActorID) {
  const sourcesToRemove = [];
  const actorsToRemove = [];

  for (const [
    sourceId,
    actorsForSource,
  ] of state.sources.mutableSourceActors.entries()) {
    let removedActorsCount = 0;
    // Find all actors for the current source which belongs to the given thread actor
    for (const actor of actorsForSource) {
      if (actor.thread == threadActorID) {
        actorsToRemove.push(actor);
        removedActorsCount++;
      }
    }

    // If we are about to remove all source actors for the current source,
    // or if for some unexpected reason we have a source with no actors,
    // notify the caller to also remove this source.
    if (
      removedActorsCount == actorsForSource.length ||
      !actorsForSource.length
    ) {
      sourcesToRemove.push(state.sources.mutableSources.get(sourceId));

      // Also remove any original sources related to this generated source
      const originalSourceIds =
        state.sources.mutableOriginalSources.get(sourceId);
      if (originalSourceIds?.length > 0) {
        for (const originalSourceId of originalSourceIds) {
          sourcesToRemove.push(
            state.sources.mutableSources.get(originalSourceId)
          );
        }
      }
    }
  }

  return {
    actors: actorsToRemove,
    sources: sourcesToRemove,
  };
}
