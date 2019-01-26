/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// This module converts Firefox specific types to the generic types

import type { Frame, Source, SourceLocation } from "../../types";
import type {
  PausedPacket,
  FramesResponse,
  FramePacket,
  SourcePayload
} from "./types";

import { clientCommands } from "./commands";

export function createFrame(frame: FramePacket): ?Frame {
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
  const location = {
    sourceId: frame.where.source ? frame.where.source.actor : frame.where.actor,
    line: frame.where.line,
    column: frame.where.column
  };

  return {
    id: frame.actor,
    displayName: title,
    location,
    generatedLocation: location,
    this: frame.this,
    scope: frame.environment
  };
}

export function createSource(
  thread: string,
  source: SourcePayload,
  { supportsWasm }: { supportsWasm: boolean }
): Source {
  const createdSource = {
    id: source.actor,
    thread,
    url: source.url,
    relativeUrl: source.url,
    isPrettyPrinted: false,
    isWasm: false,
    sourceMapURL: source.sourceMapURL,
    introductionUrl: source.introductionUrl,
    isBlackBoxed: false,
    loadedState: "unloaded"
  };
  clientCommands.registerSource(createdSource);
  return Object.assign(createdSource, {
    isWasm: supportsWasm && source.introductionType === "wasm"
  });
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
    frame: createFrame(frame),
    frames: response.frames.map(createFrame)
  };
}

// Firefox only returns `actualLocation` if it actually changed,
// but we want it always to exist. Format `actualLocation` if it
// exists, otherwise use `location`.

export function createBreakpointLocation(
  location: SourceLocation,
  actualLocation?: Object
): SourceLocation {
  if (!actualLocation) {
    return location;
  }

  return {
    sourceId: actualLocation.source.actor,
    sourceUrl: actualLocation.source.url,
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
