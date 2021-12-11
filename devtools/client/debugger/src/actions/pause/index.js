/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for the pause state
 * @module actions/pause
 */

export {
  selectThread,
  stepIn,
  stepOver,
  stepOut,
  resume,
  restart,
} from "./commands";
export { fetchFrames } from "./fetchFrames";
export { fetchScopes } from "./fetchScopes";
export { paused } from "./paused";
export { resumed } from "./resumed";
export { continueToHere } from "./continueToHere";
export { breakOnNext } from "./breakOnNext";
export { mapFrames } from "./mapFrames";
export { mapDisplayNames } from "./mapDisplayNames";
export { pauseOnExceptions } from "./pauseOnExceptions";
export { selectFrame } from "./selectFrame";
export { toggleSkipPausing, setSkipPausing } from "./skipPausing";
export { toggleMapScopes } from "./mapScopes";
export { setExpandedScope } from "./expandScopes";
export { generateInlinePreview } from "./inlinePreview";
export { highlightCalls, unhighlightCalls } from "./highlightCalls";
export {
  previewPausedLocation,
  clearPreviewPausedLocation,
} from "./previewPausedLocation";
