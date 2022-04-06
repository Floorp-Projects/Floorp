/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { asSettled } from "../utils/async-value";

/**
 * Tells if a given Source Actor is registered in the redux store
 *
 * @param {Object} state
 * @param {String} id
 *        Source Actor ID
 * @return {Boolean}
 */
export function hasSourceActor(state, id) {
  return state.sourceActors.has(id);
}

/**
 * Get the Source Actor object. See create.js:createSourceActor()
 *
 * @param {Object} state
 * @param {String} id
 *        Source Actor ID
 * @return {Object}
 *        The Source Actor object (if registered)
 */
export function getSourceActor(state, id) {
  return state.sourceActors.get(id);
}

// Used by sources selectors
/**
 * Get many Source Actor objects. See create.js:createSourceActor()
 *
 * @param {Object} state
 * @param {Array<String>} ids
 *        List of Source Actor IDs
 * @return {Array<Object>}
 *        List of sources actors
 */
export function getSourceActors(state, ids) {
  return ids.map(id => getSourceActor(state, id)).filter(source => !!source);
}

// Used by threads selectors
/**
 * Get all Source Actor objects for a given thread. See create.js:createSourceActor()
 *
 * @param {Object} state
 * @param {Array<String>} ids
 *        List of Thread IDs
 * @return {Array<Object>}
 */
export function getSourceActorsForThread(state, ids) {
  if (!Array.isArray(ids)) {
    ids = [ids];
  }
  const actors = [];
  for (const sourceActor of state.sourceActors.values()) {
    if (ids.includes(sourceActor.thread)) {
      actors.push(sourceActor);
    }
  }
  return actors;
}

/**
 * Get the list of all breakable lines for a given source actor.
 *
 * @param {Object} state
 * @param {String} id
 *        Source Actor ID
 * @return {AsyncValue<Array<Number>>}
 *        List of all the breakable lines.
 */
export function getSourceActorBreakableLines(state, id) {
  const { breakableLines } = getSourceActor(state, id);

  return asSettled(breakableLines);
}

// Used by sources selectors
/**
 * Get the list of all breakable lines for a set of source actors.
 *
 * This is typically used to fetch the breakable lines of HTML sources
 * which are made of multiple source actors (one per inline script).
 *
 * @param {Object} state
 * @param {Array<String>} ids
 *        List of Source Actor IDs
 * @return {AsyncValue<Array<Number>>}
 *        List of all the breakable lines.
 */
export function getBreakableLinesForSourceActors(state, ids) {
  const allBreakableLines = [];
  for (const id of ids) {
    const { breakableLines } = getSourceActor(state, id);
    if (breakableLines && breakableLines.state == "fulfilled") {
      allBreakableLines.push(...breakableLines.value);
    }
  }
  return allBreakableLines;
}
