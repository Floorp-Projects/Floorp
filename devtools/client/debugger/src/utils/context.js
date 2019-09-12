/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

import type { ThreadId } from "../types";
import type { State } from "../reducers/types";

import { getThreadContext } from "../selectors";

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
export type NavigateContext = {|
  // Counter reflecting how many times the debugger has navigated to a new page
  // and reset most of its state.
  +navigateCounter: number,
|};

// A ThreadContext is invalidated if the target navigates, or if the current
// thread changes, pauses, or resumes.
export type ThreadContext = {|
  +navigateCounter: number,

  // The currently selected thread.
  +thread: ThreadId,

  // Counter reflecting how many times the selected thread has paused or
  // resumed.
  +pauseCounter: number,

  // Whether the current thread is paused. This is determined from the other
  // Context properties and is here for convenient access.
  +isPaused: boolean,
|};

export type Context = NavigateContext | ThreadContext;

export class ContextError extends Error {}

export function validateNavigateContext(state: State, cx: Context) {
  const newcx = getThreadContext(state);

  if (newcx.navigateCounter != cx.navigateCounter) {
    throw new ContextError("Page has navigated");
  }
}

export function validateThreadContext(state: State, cx: ThreadContext) {
  const newcx = getThreadContext(state);

  if (cx.thread != newcx.thread) {
    throw new ContextError("Current thread has changed");
  }

  if (cx.pauseCounter != newcx.pauseCounter) {
    throw new ContextError("Current thread has paused or resumed");
  }
}

export function validateContext(state: State, cx: Context) {
  validateNavigateContext(state, cx);

  if ("thread" in cx) {
    validateThreadContext(state, (cx: any));
  }
}
