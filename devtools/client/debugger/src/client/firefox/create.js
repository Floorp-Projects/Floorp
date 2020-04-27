/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
// This module converts Firefox specific types to the generic types

import type { Frame, ThreadId, GeneratedSourceData, Thread } from "../../types";
import type {
  PausedPacket,
  FrameFront,
  SourcePayload,
  ThreadFront,
  Target,
} from "./types";

import { clientCommands } from "./commands";

export function prepareSourcePayload(
  threadFront: ThreadFront,
  source: SourcePayload
): GeneratedSourceData {
  const { isServiceWorker } = threadFront.parentFront;

  // We populate the set of sources as soon as we hear about them. Note that
  // this means that we have seen an actor, but it might still be in the
  // debounced queue for creation, so the Redux store itself might not have
  // a source actor with this ID yet.
  clientCommands.registerSourceActor(
    source.actor,
    makeSourceId(source, isServiceWorker)
  );

  source = { ...source };

  // Maintain backward-compat with servers that only return introductionUrl and
  // not sourceMapBaseURL.
  if (
    typeof source.sourceMapBaseURL === "undefined" &&
    typeof (source: any).introductionUrl !== "undefined"
  ) {
    source.sourceMapBaseURL =
      source.url || (source: any).introductionUrl || null;
    delete (source: any).introductionUrl;
  }

  return { thread: threadFront.actor, isServiceWorker, source };
}

export function createFrame(
  thread: ThreadId,
  frame: FrameFront,
  index: number = 0
): ?Frame {
  if (!frame) {
    return null;
  }

  const location = {
    sourceId: clientCommands.getSourceForActor(frame.where.actor),
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
  };
}

export function makeSourceId(source: SourcePayload, isServiceWorker: boolean) {
  // Source actors with the same URL will be given the same source ID and
  // grouped together under the same source in the client. There is an exception
  // for sources from service workers, where there may be multiple service
  // worker threads running at the same time which use different versions of the
  // same URL.
  return source.url && !isServiceWorker
    ? `sourceURL-${source.url}`
    : `source-${source.actor}`;
}

export function createPause(thread: string, packet: PausedPacket): any {
  return {
    ...packet,
    thread,
    frame: createFrame(thread, packet.frame),
  };
}

function getTargetType(target: Target) {
  if (target.isWorkerTarget) {
    return "worker";
  }

  if (target.isContentProcess) {
    return "contentProcess";
  }

  return "mainThread";
}

export function createThread(actor: string, target: Target): Thread {
  return {
    actor,
    url: target.url,
    type: getTargetType(target),
    name: target.name,
    serviceWorkerStatus: target.debuggerServiceWorkerStatus,
  };
}
