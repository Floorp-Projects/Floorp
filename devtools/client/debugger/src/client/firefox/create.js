/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This module converts Firefox specific types to the generic types

import { clientCommands } from "./commands";
import { hasSourceActor, getSourceActor } from "../../selectors";
import { stringToSourceActorId } from "../../reducers/source-actors";

let store;

/**
 * This function is to be called first before any other
 * and allow having access to any instances of classes that are
 * useful for this module
 *
 * @param {Object} dependencies
 * @param {Object} dependencies.store
 *                 The redux store object of the debugger frontend.
 */
export function setupCreate(dependencies) {
  store = dependencies.store;
}

export function prepareSourcePayload(threadFront, source) {
  source = { ...source };

  // Maintain backward-compat with servers that only return introductionUrl and
  // not sourceMapBaseURL.
  if (
    typeof source.sourceMapBaseURL === "undefined" &&
    typeof source.introductionUrl !== "undefined"
  ) {
    source.sourceMapBaseURL = source.url || source.introductionUrl || null;
    delete source.introductionUrl;
  }

  return { thread: threadFront.actor, source };
}

export async function createFrame(thread, frame, index = 0) {
  if (!frame) {
    return null;
  }

  // Because of throttling, the source may be available a bit late.
  const source = await waitForSourceActorToBeRegisteredInStore(
    frame.where.actor
  );

  const location = {
    sourceId: makeSourceId(source, thread),
    line: frame.where.line,
    column: frame.where.column,
  };

  return {
    id: frame.actorID,
    thread,
    displayName: frame.displayName,
    location,
    generatedLocation: location,
    this: frame.this,
    source: null,
    index,
    asyncCause: frame.asyncCause,
    state: frame.state,
    type: frame.type,
  };
}

/**
 * This method wait for the given source to be registered in Redux store.
 *
 * @param {String} sourceActor
 *                 Actor ID of the source to be waiting for.
 */
async function waitForSourceActorToBeRegisteredInStore(sourceActorIdString) {
  const sourceActorId = stringToSourceActorId(sourceActorIdString);
  if (!hasSourceActor(store.getState(), sourceActorId)) {
    await new Promise(resolve => {
      const unsubscribe = store.subscribe(check);
      let currentState = null;
      function check() {
        const previousState = currentState;
        currentState = store.getState().sourceActors.values;
        // For perf reason, avoid any extra computation if sources did not change
        if (previousState == currentState) {
          return;
        }
        if (hasSourceActor(store.getState(), sourceActorId)) {
          unsubscribe();
          resolve();
        }
      }
    });
  }
  return getSourceActor(store.getState(), sourceActorId);
}

export function makeSourceId(source, threadActorId) {
  // Source actors with the same URL will be given the same source ID and
  // grouped together under the same source in the client. There is an exception
  // for sources from distinct target types, where there may be multiple processes/threads
  // running at the same time which use different versions of the same URL.
  const target = clientCommands.lookupTarget(threadActorId);
  if (target.isTopLevel && source.url) {
    return `source-${source.url}`;
  }
  return `source-${source.actor}`;
}

export async function createPause(thread, packet) {
  const frame = await createFrame(thread, packet.frame);
  return {
    ...packet,
    thread,
    frame,
  };
}

export function createThread(actor, target) {
  const name = target.isTopLevel ? L10N.getStr("mainThread") : target.name;

  return {
    actor,
    url: target.url,
    isTopLevel: target.isTopLevel,
    targetType: target.targetType,
    name,
    serviceWorkerStatus: target.debuggerServiceWorkerStatus,
  };
}
