/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require('../constants');
const promise = require('promise');
const { asPaused } = require('../utils');
const { PROMISE } = require('devtools/client/shared/redux/middleware/promise');
const {
  getSource, getBreakpoint, getBreakpoints, makeLocationId
} = require('../queries');

// Because breakpoints are just simple data structures, we still need
// a way to lookup the actual client instance to talk to the server.
// We keep an internal database of clients based off of actor ID.
const BREAKPOINT_CLIENT_STORE = new Map();

function setBreakpointClient(actor, client) {
  BREAKPOINT_CLIENT_STORE.set(actor, client);
}

function getBreakpointClient(actor) {
  return BREAKPOINT_CLIENT_STORE.get(actor);
}

function enableBreakpoint(location) {
  // Enabling is exactly the same as adding. It will use the existing
  // breakpoint that still stored.
  return addBreakpoint(location);
}

function _breakpointExists(state, location) {
  const currentBp = getBreakpoint(state, location);
  return currentBp && !currentBp.disabled;
}

function _getOrCreateBreakpoint(state, location, condition) {
  return getBreakpoint(state, location) || { location, condition };
}

function addBreakpoint(location, condition) {
  return (dispatch, getState) => {
    if (_breakpointExists(getState(), location)) {
      return;
    }

    const bp = _getOrCreateBreakpoint(getState(), location, condition);

    return dispatch({
      type: constants.ADD_BREAKPOINT,
      breakpoint: bp,
      condition: condition,
      [PROMISE]: Task.spawn(function*() {
        const sourceClient = gThreadClient.source(
          getSource(getState(), bp.location.actor)
        );
        const [response, bpClient] = yield sourceClient.setBreakpoint({
          line: bp.location.line,
          column: bp.location.column,
          condition: bp.condition
        });
        const { isPending, actualLocation } = response;

        // Save the client instance
        setBreakpointClient(bpClient.actor, bpClient);

        return {
          text: DebuggerView.editor.getText(bp.location.line - 1).trim(),

          // If the breakpoint response has an "actualLocation" attached, then
          // the original requested placement for the breakpoint wasn't
          // accepted.
          actualLocation: isPending ? null : actualLocation,
          actor: bpClient.actor
        };
      })
    });
  }
}

function disableBreakpoint(location) {
  return _removeOrDisableBreakpoint(location, true);
}

function removeBreakpoint(location) {
  return _removeOrDisableBreakpoint(location);
}

function _removeOrDisableBreakpoint(location, isDisabled) {
  return (dispatch, getState) => {
    let bp = getBreakpoint(getState(), location);
    if (!bp) {
      throw new Error('attempt to remove breakpoint that does not exist');
    }
    if (bp.loading) {
      // TODO(jwl): make this wait until the breakpoint is saved if it
      // is still loading
      throw new Error('attempt to remove unsaved breakpoint');
    }

    const bpClient = getBreakpointClient(bp.actor);

    return dispatch({
      type: constants.REMOVE_BREAKPOINT,
      breakpoint: bp,
      disabled: isDisabled,
      [PROMISE]: bpClient.remove()
    });
  }
}

function removeAllBreakpoints() {
  return (dispatch, getState) => {
    const breakpoints = getBreakpoints(getState());
    const activeBreakpoints = breakpoints.filter(bp => !bp.disabled);
    activeBreakpoints.forEach(bp => removeBreakpoint(bp.location));
  }
}

/**
 * Update the condition of a breakpoint.
 *
 * @param object aLocation
 *        @see DebuggerController.Breakpoints.addBreakpoint
 * @param string aClients
 *        The condition to set on the breakpoint
 * @return object
 *         A promise that will be resolved with the breakpoint client
 */
function setBreakpointCondition(location, condition) {
  return (dispatch, getState) => {
    const bp = getBreakpoint(getState(), location);
    if (!bp) {
      throw new Error("Breakpoint does not exist at the specified location");
    }
    if (bp.loading){
      // TODO(jwl): when this function is called, make sure the action
      // creator waits for the breakpoint to exist
      throw new Error("breakpoint must be saved");
    }

    const bpClient = getBreakpointClient(bp.actor);

    return dispatch({
      type: constants.SET_BREAKPOINT_CONDITION,
      breakpoint: bp,
      condition: condition,
      [PROMISE]: Task.spawn(function*() {
        const newClient = yield bpClient.setCondition(gThreadClient, condition);

        // Remove the old instance and save the new one
        setBreakpointClient(bpClient.actor, null);
        setBreakpointClient(newClient.actor, newClient);

        return { actor: newClient.actor };
      })
    });
  };
}

module.exports = {
  enableBreakpoint,
  addBreakpoint,
  disableBreakpoint,
  removeBreakpoint,
  removeAllBreakpoints,
  setBreakpointCondition
}
