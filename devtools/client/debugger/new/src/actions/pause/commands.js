"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.command = command;
exports.stepIn = stepIn;
exports.stepOver = stepOver;
exports.stepOut = stepOut;
exports.resume = resume;
exports.rewind = rewind;
exports.reverseStepIn = reverseStepIn;
exports.reverseStepOver = reverseStepOver;
exports.reverseStepOut = reverseStepOut;
exports.astCommand = astCommand;

var _selectors = require("../../selectors/index");

var _promise = require("../utils/middleware/promise");

var _parser = require("../../workers/parser/index");

var _breakpoints = require("../breakpoints");

var _prefs = require("../../utils/prefs");

/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Debugger commands like stepOver, stepIn, stepUp
 *
 * @param string $0.type
 * @memberof actions/pause
 * @static
 */
function command(type) {
  return async ({
    dispatch,
    client
  }) => {
    return dispatch({
      type: "COMMAND",
      command: type,
      [_promise.PROMISE]: client[type]()
    });
  };
}
/**
 * StepIn
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function stepIn() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("stepIn"));
    }
  };
}
/**
 * stepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function stepOver() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(astCommand("stepOver"));
    }
  };
}
/**
 * stepOut
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function stepOut() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("stepOut"));
    }
  };
}
/**
 * resume
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function resume() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("resume"));
    }
  };
}
/**
 * rewind
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function rewind() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("rewind"));
    }
  };
}
/**
 * reverseStepIn
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function reverseStepIn() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("reverseStepIn"));
    }
  };
}
/**
 * reverseStepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function reverseStepOver() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(astCommand("reverseStepOver"));
    }
  };
}
/**
 * reverseStepOut
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */


function reverseStepOut() {
  return ({
    dispatch,
    getState
  }) => {
    if ((0, _selectors.isPaused)(getState())) {
      return dispatch(command("reverseStepOut"));
    }
  };
}
/*
 * Checks for await or yield calls on the paused line
 * This avoids potentially expensive parser calls when we are likely
 * not at an async expression.
 */


function hasAwait(source, pauseLocation) {
  const {
    line,
    column
  } = pauseLocation;

  if (!source.text) {
    return false;
  }

  const lineText = source.text.split("\n")[line - 1];

  if (!lineText) {
    return false;
  }

  const snippet = lineText.slice(column - 50, column + 50);
  return !!snippet.match(/(yield|await)/);
}
/**
 * @memberOf actions/pause
 * @static
 * @param stepType
 * @returns {function(ThunkArgs)}
 */


function astCommand(stepType) {
  return async ({
    dispatch,
    getState,
    sourceMaps
  }) => {
    if (!_prefs.features.asyncStepping) {
      return dispatch(command(stepType));
    }

    if (stepType == "stepOver") {
      // This type definition is ambiguous:
      const frame = (0, _selectors.getTopFrame)(getState());
      const source = (0, _selectors.getSource)(getState(), frame.location.sourceId);

      if (source && hasAwait(source, frame.location)) {
        const nextLocation = await (0, _parser.getNextStep)(source.id, frame.location);

        if (nextLocation) {
          await dispatch((0, _breakpoints.addHiddenBreakpoint)(nextLocation));
          return dispatch(command("resume"));
        }
      }
    }

    return dispatch(command(stepType));
  };
}