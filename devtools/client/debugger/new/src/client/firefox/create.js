/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// This module converts Firefox specific types to the generic types

import type { Frame, Source, SourceActorLocation, ThreadId } from "../../types";
import type {
  PausedPacket,
  FramesResponse,
  FramePacket,
  SourcePayload,
  CreateSourceResult
} from "./types";

import { clientCommands } from "./commands";

export function createFrame(thread: ThreadId, frame: FramePacket): ?Frame {
  if (!frame) {
    return null;
  }
  let title;
  if (frame.type == "call") {
    const c = frame.callee;
    title = c.name || c.userDisplayName || c.displayName;
  } else {
    title = `(${frame.type})`;
  }

  // NOTE: Firefox 66 switched from where.source to where.actor
  const actor = frame.where.source
    ? frame.where.source.actor
    : frame.where.actor;

  const location = {
    sourceId: clientCommands.getSourceForActor(actor),
    line: frame.where.line,
    column: frame.where.column
  };

  return {
    id: frame.actor,
    thread,
    displayName: title,
    location,
    generatedLocation: location,
    this: frame.this,
    source: null,
    scope: frame.environment
  };
}

function makeSourceId(source) {
  return source.url ? `sourceURL-${source.url}` : `source-${source.actor}`;
}

export function createSource(
  thread: string,
  source: SourcePayload,
  { supportsWasm }: { supportsWasm: boolean }
): CreateSourceResult {
  const createdSource: any = {
    id: makeSourceId(source),
    url: source.url,
    relativeUrl: source.url,
    isPrettyPrinted: false,
    sourceMapURL: source.sourceMapURL,
    introductionUrl: source.introductionUrl,
    isBlackBoxed: false,
    loadedState: "unloaded",
    isWasm: supportsWasm && source.introductionType === "wasm"
  };
  const sourceActor = {
    actor: source.actor,
    source: createdSource.id,
    thread
  };
  clientCommands.registerSourceActor(sourceActor);
  return { sourceActor, source: (createdSource: Source) };
}

export function createPause(
  thread: string,
  packet: PausedPacket,
  response: FramesResponse
): any {
  // NOTE: useful when the debugger is already paused
  const frame = packet.frame || response.frames[0];

  return {
    ...packet,
    thread,
    frame: createFrame(thread, frame),
    frames: response.frames.map(createFrame.bind(null, thread))
  };
}

// Firefox only returns `actualLocation` if it actually changed,
// but we want it always to exist. Format `actualLocation` if it
// exists, otherwise use `location`.

export function createBreakpointLocation(
  location: SourceActorLocation,
  actualLocation?: Object
): SourceActorLocation {
  if (!actualLocation) {
    return location;
  }

  return {
    ...location,
    line: actualLocation.line,
    column: actualLocation.column
  };
}

export function createWorker(actor: string, url: string) {
  return {
    actor,
    url,
    // Ci.nsIWorkerDebugger.TYPE_DEDICATED
    type: 0
  };
}
