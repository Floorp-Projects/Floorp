/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// This module converts Firefox specific types to the generic types

import type { Frame, ThreadId, GeneratedSourceData, Thread } from "../../types";
import type {
  PausedPacket,
  FramesResponse,
  FramePacket,
  SourcePayload,
  ThreadFront,
  Target,
} from "./types";

import { clientCommands } from "./commands";

export function prepareSourcePayload(
  client: ThreadFront,
  source: SourcePayload
): GeneratedSourceData {
  // We populate the set of sources as soon as we hear about them. Note that
  // this means that we have seen an actor, but it might still be in the
  // debounced queue for creation, so the Redux store itself might not have
  // a source actor with this ID yet.
  clientCommands.registerSourceActor(source.actor, makeSourceId(source));

  return { thread: client.actor, source };
}

export function createFrame(thread: ThreadId, frame: FramePacket): ?Frame {
  if (!frame) {
    return null;
  }

  const location = {
    sourceId: clientCommands.getSourceForActor(frame.where.actor),
    line: frame.where.line,
    column: frame.where.column,
  };

  return {
    id: frame.actor,
    thread,
    displayName: frame.displayName,
    location,
    generatedLocation: location,
    this: frame.this,
    source: null,
    scope: frame.environment,
  };
}

export function makeSourceId(source: SourcePayload) {
  return source.url ? `sourceURL-${source.url}` : `source-${source.actor}`;
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
    frames: response.frames.map(createFrame.bind(null, thread)),
  };
}

export function createThread(actor: string, target: Target): Thread {
  return {
    actor,
    url: target.url || "",
    // Ci.nsIWorkerDebugger.TYPE_DEDICATED
    type: actor.includes("process") ? 1 : 0,
    name: "",
  };
}
