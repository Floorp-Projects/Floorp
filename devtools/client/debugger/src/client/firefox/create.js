/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This module converts Firefox specific types to the generic types

import { hasSourceActor, getSourceActor } from "../../selectors";
import { features } from "../../utils/prefs";
import { isUrlExtension } from "../../utils/source";

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

export async function createFrame(thread, frame, index = 0) {
  if (!frame) {
    return null;
  }

  // Because of throttling, the source may be available a bit late.
  const sourceActor = await waitForSourceActorToBeRegisteredInStore(
    frame.where.actor
  );

  const location = {
    sourceId: sourceActor.source,
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
async function waitForSourceActorToBeRegisteredInStore(sourceActorId) {
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

// Compute the reducer's source ID for a given source front/resource.
//
// We have four kind of "sources":
//   * "sources" in sources.js reducer, which map to 1 or many:
//   * "source actors" in source-actors.js reducer, which map 1 for 1 with:
//   * "SOURCE" resources coming from ResourceCommand API
//   * SourceFront, which are retrieved via `ThreadFront.source(sourceResource)`
//
// Note that SOURCE resources are actually the "form" of the SourceActor,
// with the addition of `resourceType` and `targetFront` attributes.
//
// Unfortunately, the debugger frontend interacts with these 4 type of objects.
// The last three actually try to represent the exact same thing.
//
// Here this method received a SOURCE resource (the 3rd bullet point)
export function makeSourceId(sourceResource) {
  // Allows Jest to use custom, simplier IDs
  if ("mockedJestID" in sourceResource) {
    return sourceResource.mockedJestID;
  }
  // Source actors with the same URL will be given the same source ID and
  // grouped together under the same source in the client. There is an exception
  // for sources from distinct target types, where there may be multiple processes/threads
  // running at the same time which use different versions of the same URL.
  if (sourceResource.targetFront.isTopLevel && sourceResource.url) {
    return `source-${sourceResource.url}`;
  }
  return `source-${sourceResource.actor}`;
}

/**
 * Create the source object for a generated source that is stored in sources.js reducer.
 * These generated sources relate to JS code which run in the
 * debugged runtime (as oppose to original sources
 * which are only available in debugger's environment).
 *
 * @param {SOURCE} sourceResource
 *        SOURCE resource coming from the ResourceCommand API.
 *        This represents the `SourceActor` from the server codebase.
 */
export function createGeneratedSource(sourceResource) {
  return createSourceObject({
    id: makeSourceId(sourceResource),
    url: sourceResource.url,
    extensionName: sourceResource.extensionName,
    isWasm: !!features.wasm && sourceResource.introductionType === "wasm",
    isExtension:
      (sourceResource.url && isUrlExtension(sourceResource.url)) || false,
  });
}

/**
 * Create the source object that is stored in sources.js reducer.
 *
 * This is an internal helper to this module to ensure all sources have the same shape.
 * Do not use it outside of this module!
 */
function createSourceObject({
  id,
  url,
  extensionName = null,
  isWasm = false,
  isExtension = false,
  isPrettyPrinted = false,
  isOriginal = false,
}) {
  return {
    // The ID, computed by:
    // * `makeSourceId` for generated,
    // * `generatedToOriginalId` for both source map and pretty printed original,
    id,

    // Absolute URL for the source. This may be a fake URL for pretty printed sources
    url,

    // By default refers to the absolute URL, but this will be updated
    // if user defines a project root. In this case it will be crafted via `getRelativeUrl`
    // to refer to the relative path based on project root.
    // (Note that this will rather be a path than a URL)
    relativeUrl: url,

    // Only set for generated sources that are WebExtension sources.
    // This is especially useful to display the extension name for content scripts
    // that executes against the page we are debugging.
    extensionName,

    // Will be true if the source URL starts with moz-extension://,
    // which most likely means the source is a content script.
    // (Note that when debugging an add-on all generated sources will most likely have this flag set to true)
    isExtension,

    // True if WASM is enabled *and* the generated source is a WASM source
    isWasm,

    // True, if this is an original pretty printed source
    isPrettyPrinted,

    // True for source map original files, as well as pretty printed sources
    isOriginal,

    // By default set to false for all sources, but may later be toggled to true
    // if the whole source is blackboxed.
    isBlackBoxed: false,
  };
}

/**
 * Create the source object for a source mapped original source that is stored in sources.js reducer.
 * These original sources referred to by source maps.
 * This isn't code that runs in the runtime, so it isn't associated with anything
 * on the server side. It is associated with a generated source for the related bundle file
 * which itself relates to an actual code that runs in the runtime.
 *
 * @param {String} id
 *        The ID of the source, computed by source map codebase.
 * @param {String} url
 *        The URL of the original source file.
 */
export function createSourceMapOriginalSource(id, url) {
  return createSourceObject({
    id,
    url,
    isOriginal: true,
  });
}

/**
 * Create the source object for a pretty printed original source that is stored in sources.js reducer.
 * These original pretty printed sources aren't code that run in the runtime,
 * so it isn't associated with anything on the server side.
 * It is associated with a generated source for the non-pretty-printed file
 * which itself relates to an actual code that runs in the runtime.
 *
 * @param {String} id
 *        The ID of the source, computed by pretty print.
 * @param {String} url
 *        The URL of the pretty-printed source file.
 *        This URL doesn't work. It is the URL of the non-pretty-printed file with ":formated" suffix.
 */
export function createPrettyPrintOriginalSource(id, url) {
  return createSourceObject({
    id,
    url,
    isOriginal: true,
    isPrettyPrinted: true,
  });
}

/**
 * Create the "source actor" object that is stored in source-actor.js reducer.
 * This will represent server's source actor in the reducer universe.
 *
 * @param {SOURCE} sourceResource
 *        SOURCE resource coming from the ResourceCommand API.
 *        This represents the `SourceActor` from the server codebase.
 */
export function createSourceActor(sourceResource) {
  const actorId = sourceResource.actor;

  // As sourceResource is only SourceActor's form and not the SourceFront,
  // we have to go through the target to retrieve the related ThreadActor's ID.
  const threadActorID = sourceResource.targetFront.getCachedFront("thread")
    .actorID;

  return {
    id: actorId,
    actor: actorId,
    thread: threadActorID,
    // `source` is the reducer source ID
    source: makeSourceId(sourceResource),
    sourceMapBaseURL: sourceResource.sourceMapBaseURL,
    sourceMapURL: sourceResource.sourceMapURL,
    url: sourceResource.url,
    introductionType: sourceResource.introductionType,
  };
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
