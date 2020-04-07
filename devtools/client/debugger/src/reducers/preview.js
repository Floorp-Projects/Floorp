/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { AstLocation } from "../workers/parser";

import type { Action } from "../actions/types";
import type { Node, Grip } from "devtools-reps";

export type Preview = {|
  expression: string,
  resultGrip: Grip | null,
  root: Node,
  properties: Array<Grip>,
  location: AstLocation,
  cursorPos: any,
  tokenPos: AstLocation,
  target: HTMLDivElement,
|};

export type PreviewState = {
  +preview: ?Preview,
  previewCount: number,
};

export function initialPreviewState(): PreviewState {
  return {
    preview: null,
    previewCount: 0,
  };
}

function update(
  state: PreviewState = initialPreviewState(),
  action: Action
): PreviewState {
  switch (action.type) {
    case "CLEAR_PREVIEW": {
      return { ...state, preview: null };
    }

    case "START_PREVIEW": {
      return { ...state, previewCount: state.previewCount + 1 };
    }

    case "SET_PREVIEW": {
      return { ...state, preview: action.value };
    }
  }

  return state;
}

// NOTE: we'd like to have the app state fully typed
// https://github.com/firefox-devtools/debugger/blob/master/src/reducers/sources.js#L179-L185
type OuterState = { preview: PreviewState };

export function getPreview(state: OuterState): ?Preview {
  return state.preview.preview;
}

export function getPreviewCount(state: OuterState): number {
  return state.preview.previewCount;
}

export default update;
