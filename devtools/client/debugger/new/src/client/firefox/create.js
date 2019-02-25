/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// This module converts Firefox specific types to the generic types

import { isUrlExtension } from "../../utils/source";

import type { Frame, Source, ThreadId } from "../../types";
import type {
  PausedPacket,
  FramesResponse,
  FramePacket,
  SourcePayload
} from "./types";

import { clientCommands } from "./commands";

export function createFrame(thread: ThreadId, frame: FramePacket): ?Frame {
  if (!frame) {
    return null;
  }

  const location = {
    sourceId: clientCommands.getSourceForActor(frame.where.actor),
    line: frame.where.line,
    column: frame.where.column
  };

  return {
    id: frame.actor,
    thread,
    displayName: frame.displayName,
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
): Source {
  const id = makeSourceId(source);
  const sourceActor = {
    actor: source.actor,
    source: id,
    thread
  };
  const createdSource: any = {
    id,
    url: source.url,
    relativeUrl: source.url,
    isPrettyPrinted: false,
    sourceMapURL: source.sourceMapURL,
    introductionUrl: source.introductionUrl,
    isBlackBoxed: false,
    loadedState: "unloaded",
    isWasm: supportsWasm && source.introductionType === "wasm",
    isExtension: (source.url && isUrlExtension(source.url)) || false,
    actors: [sourceActor]
  };
  clientCommands.registerSourceActor(sourceActor);
  return createdSource;
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

export function createWorker(actor: string, url: string) {
  return {
    actor,
    url,
    // Ci.nsIWorkerDebugger.TYPE_DEDICATED
    type: 0
  };
}
