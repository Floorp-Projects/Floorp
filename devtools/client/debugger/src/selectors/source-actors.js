/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tells if a given Source Actor is registered in the redux store
 *
 * @param {Object} state
 * @param {String} sourceActorId
 *        Source Actor ID
 * @return {Boolean}
 */
export function hasSourceActor(state, sourceActorId) {
  return state.sourceActors.mutableSourceActors.has(sourceActorId);
}

/**
 * Get the Source Actor object. See create.js:createSourceActor()
 *
 * @param {Object} state
 * @param {String} sourceActorId
 *        Source Actor ID
 * @return {Object}
 *        The Source Actor object (if registered)
 */
export function getSourceActor(state, sourceActorId) {
  return state.sourceActors.mutableSourceActors.get(sourceActorId);
}

/**
 * Reports if the Source Actor relates to a valid source map / original source.
 *
 * @param {Object} state
 * @param {String} sourceActorId
 *        Source Actor ID
 * @return {Boolean}
 *        True if it has a valid source map/original object.
 */
export function isSourceActorWithSourceMap(state, sourceActorId) {
  return state.sourceActors.mutableSourceActorsWithSourceMap.has(sourceActorId);
}

export function getSourceMapErrorForSourceActor(state, sourceActorId) {
  return state.sourceActors.mutableSourceMapErrors.get(sourceActorId);
}

export function getSourceMapResolvedURL(state, sourceActorId) {
  return state.sourceActors.mutableResolvedSourceMapURL.get(sourceActorId);
}

// Used by threads selectors
/**
 * Get all Source Actor objects for a given thread. See create.js:createSourceActor()
 *
 * @param {Object} state
 * @param {Array<String>} threadActorIDs
 *        List of Thread IDs
 * @return {Array<Object>}
 */
export function getSourceActorsForThread(state, threadActorIDs) {
  if (!Array.isArray(threadActorIDs)) {
    threadActorIDs = [threadActorIDs];
  }
  const actors = [];
  for (const sourceActor of state.sourceActors.mutableSourceActors.values()) {
    if (threadActorIDs.includes(sourceActor.thread)) {
      actors.push(sourceActor);
    }
  }
  return actors;
}

/**
 * Get the list of all breakable lines for a given source actor.
 *
 * @param {Object} state
 * @param {String} sourceActorId
 *        Source Actor ID
 * @return {AsyncValue<Array<Number>>}
 *        List of all the breakable lines.
 */
export function getSourceActorBreakableLines(state, sourceActorId) {
  return state.sourceActors.mutableBreakableLines.get(sourceActorId);
}

// Used by sources selectors
/**
 * Get the list of all breakable lines for a set of source actors.
 *
 * This is typically used to fetch the breakable lines of HTML sources
 * which are made of multiple source actors (one per inline script).
 *
 * @param {Object} state
 * @param {Array<String>} sourceActors
 *        List of Source Actors
 * @param {Boolean} isHTML
 *        True, if we are fetching the breakable lines for an HTML source.
 *        For them, we have to aggregate the lines of each source actors.
 *        Otherwise, we might still have many source actors, but one per thread.
 *        In this case, we simply return the first source actor to have the lines ready.
 * @return {Array<Number>}
 *        List of all the breakable lines.
 */
export function getBreakableLinesForSourceActors(state, sourceActors, isHTML) {
  const allBreakableLines = [];
  for (const sourceActor of sourceActors) {
    const breakableLines = state.sourceActors.mutableBreakableLines.get(
      sourceActor.id
    );
    if (breakableLines) {
      if (isHTML) {
        allBreakableLines.push(...breakableLines);
      } else {
        return breakableLines;
      }
    }
  }
  return allBreakableLines;
}
