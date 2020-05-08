/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getFrames, getSymbols, getSource } from "../../selectors";

import { findClosestFunction } from "../../utils/ast";

import type { Frame, ThreadContext } from "../../types";
import type { ThunkArgs } from "../types";

function mapDisplayName(frame: Frame, { getState }) {
  if (frame.isOriginal) {
    return frame;
  }

  const source = getSource(getState(), frame.location.sourceId);

  if (!source) {
    return frame;
  }

  const symbols = getSymbols(getState(), source);

  if (!symbols || !symbols.functions) {
    return frame;
  }

  const originalFunction = findClosestFunction(symbols, frame.location);

  if (!originalFunction) {
    return frame;
  }

  const originalDisplayName = originalFunction.name;
  return { ...frame, originalDisplayName };
}

export function mapDisplayNames(cx: ThreadContext) {
  return function({ dispatch, getState }: ThunkArgs) {
    const frames = getFrames(getState(), cx.thread);

    if (!frames) {
      return;
    }

    const mappedFrames = frames.map(frame =>
      mapDisplayName(frame, { getState })
    );

    dispatch({
      type: "MAP_FRAME_DISPLAY_NAMES",
      cx,
      thread: cx.thread,
      frames: mappedFrames,
    });
  };
}
