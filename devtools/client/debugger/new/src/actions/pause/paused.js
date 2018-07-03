"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.paused = paused;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _selectors = require("../../selectors/index");

var _ = require("./index");

var _breakpoints = require("../breakpoints");

var _expressions = require("../expressions");

var _sources = require("../sources/index");

var _loadSourceText = require("../sources/loadSourceText");

var _ui = require("../ui");

var _commands = require("./commands");

var _pause = require("../../utils/pause/index");

var _mapFrames = require("./mapFrames");

var _fetchScopes = require("./fetchScopes");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
async function getOriginalSourceForFrame(state, frame) {
  return (0, _selectors.getSources)(state)[frame.location.sourceId];
}
/**
 * Debugger has just paused
 *
 * @param {object} pauseInfo
 * @memberof actions/pause
 * @static
 */


function paused(pauseInfo) {
  return async function ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) {
    const {
      frames,
      why,
      loadedObjects
    } = pauseInfo;
    const topFrame = frames.length > 0 ? frames[0] : null;

    if (topFrame && why.type == "resumeLimit") {
      const mappedFrame = await (0, _mapFrames.updateFrameLocation)(topFrame, sourceMaps);
      const source = await getOriginalSourceForFrame(getState(), mappedFrame); // Ensure that the original file has loaded if there is one.

      await dispatch((0, _loadSourceText.loadSourceText)(source));

      if ((0, _pause.shouldStep)(mappedFrame, getState(), sourceMaps)) {
        dispatch((0, _commands.command)("stepOver"));
        return;
      }
    }

    dispatch({
      type: "PAUSED",
      why,
      frames,
      selectedFrameId: topFrame ? topFrame.id : undefined,
      loadedObjects: loadedObjects || []
    });
    const hiddenBreakpointLocation = (0, _selectors.getHiddenBreakpointLocation)(getState());

    if (hiddenBreakpointLocation) {
      dispatch((0, _breakpoints.removeBreakpoint)(hiddenBreakpointLocation));
    }

    await dispatch((0, _.mapFrames)());
    const selectedFrame = (0, _selectors.getSelectedFrame)(getState());

    if (selectedFrame) {
      const visibleFrame = (0, _selectors.getVisibleSelectedFrame)(getState());
      const location = visibleFrame && (0, _devtoolsSourceMap.isGeneratedId)(visibleFrame.location.sourceId) ? selectedFrame.generatedLocation : selectedFrame.location;
      await dispatch((0, _sources.selectLocation)(location));
    }

    dispatch((0, _ui.togglePaneCollapse)("end", false));
    await dispatch((0, _fetchScopes.fetchScopes)()); // Run after fetching scoping data so that it may make use of the sourcemap
    // expression mappings for local variables.

    const atException = why.type == "exception";

    if (!atException || !(0, _selectors.isEvaluatingExpression)(getState())) {
      await dispatch((0, _expressions.evaluateExpressions)());
    }
  };
}