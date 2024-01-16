/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getThreadContext,
  getSelectedFrame,
  getCurrentThread,
  hasSource,
  hasSourceActor,
  getCurrentlyFetchedTopFrame,
  hasFrame,
} from "../selectors";

// Context encapsulates the main parameters of the current redux state, which
// impact most other information tracked by the debugger.
//
// The main use of Context is to control when asynchronous operations are
// allowed to make changes to the program state. Such operations might be
// invalidated as the state changes from the time the operation was originally
// initiated. For example, operations on pause state might still continue even
// after the thread unpauses.
//
// The methods below can be used to compare an old context with the current one
// and see if the operation is now invalid and should be abandoned. Actions can
// also include a 'cx' Context property, which will be checked by the context
// middleware. If the action fails validateContextAction() then it will not be
// dispatched.
//
// Context can additionally be used as a shortcut to access the main properties
// of the pause state.

// A normal Context is invalidated if the target navigates.

// A ThreadContext is invalidated if the target navigates, or if the current
// thread changes, pauses, or resumes.

export class ContextError extends Error {
  constructor(msg) {
    // Use a prefix string to help `PromiseTestUtils.allowMatchingRejectionsGlobally`
    // ignore all these exceptions as this is based on error strings.
    super(`DebuggerContextError: ${msg}`);
  }
}

export function validateNavigateContext(state, cx) {
  const newcx = getThreadContext(state);

  if (newcx.navigateCounter != cx.navigateCounter) {
    throw new ContextError("Page has navigated");
  }
}

export function validateThreadContext(state, cx) {
  const newcx = getThreadContext(state);

  if (cx.thread != newcx.thread) {
    throw new ContextError("Current thread has changed");
  }

  if (cx.pauseCounter != newcx.pauseCounter) {
    throw new ContextError("Current thread has paused or resumed");
  }
}

export function validateContext(state, cx) {
  validateNavigateContext(state, cx);

  if ("thread" in cx) {
    validateThreadContext(state, cx);
  }
}

export function validateSelectedFrame(state, selectedFrame) {
  const newThread = getCurrentThread(state);
  if (selectedFrame.thread != newThread) {
    throw new ContextError("Selected thread has changed");
  }

  const newSelectedFrame = getSelectedFrame(state, newThread);
  // Compare frame's IDs as frame objects are cloned during mapping
  if (selectedFrame.id != newSelectedFrame?.id) {
    throw new ContextError("Selected frame changed");
  }
}

export function validateBreakpoint(state, breakpoint) {
  // XHR breakpoint don't use any location and are always valid
  if (!breakpoint.location) {
    return;
  }

  if (!hasSource(state, breakpoint.location.source.id)) {
    throw new ContextError(
      `Breakpoint's location is obsolete (source '${breakpoint.location.source.id}' no longer exists)`
    );
  }
  if (!hasSource(state, breakpoint.generatedLocation.source.id)) {
    throw new ContextError(
      `Breakpoint's generated location is obsolete (source '${breakpoint.generatedLocation.source.id}' no longer exists)`
    );
  }
}

export function validateSource(state, source) {
  if (!hasSource(state, source.id)) {
    throw new ContextError(
      `Obsolete source (source '${source.id}' no longer exists)`
    );
  }
}

export function validateSourceActor(state, sourceActor) {
  if (!hasSourceActor(state, sourceActor.id)) {
    throw new ContextError(
      `Obsolete source actor (source '${sourceActor.id}' no longer exists)`
    );
  }
}

export function validateThreadFrames(state, thread, frames) {
  const newThread = getCurrentThread(state);
  if (thread != newThread) {
    throw new ContextError("Selected thread has changed");
  }
  const newTopFrame = getCurrentlyFetchedTopFrame(state, newThread);
  if (newTopFrame?.id != frames[0].id) {
    throw new ContextError("Thread moved to another location");
  }
}

export function validateFrame(state, frame) {
  if (!hasFrame(state, frame)) {
    throw new ContextError(
      `Obsolete frame (frame '${frame.id}' no longer exists)`
    );
  }
}

export function isValidThreadContext(state, cx) {
  const newcx = getThreadContext(state);
  return cx.thread == newcx.thread && cx.pauseCounter == newcx.pauseCounter;
}
