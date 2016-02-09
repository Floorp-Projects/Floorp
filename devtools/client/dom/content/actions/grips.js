/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 /* globals DomProvider */
"use strict";

const constants = require("../constants");

/**
 * Used to fetch grip prototype and properties from the backend.
 */
function requestProperties(grip) {
  return {
    grip: grip,
    type: constants.FETCH_PROPERTIES,
    status: "start",
    error: false
  };
}

/**
 * Executed when grip properties are received from the backend.
 */
function receiveProperties(grip, response, error) {
  return {
    grip: grip,
    type: constants.FETCH_PROPERTIES,
    status: "end",
    response: response,
    error: error
  };
}

/**
 * Used to get properties from the backend and fire an action
 * when they are received.
 */
function fetchProperties(grip) {
  return dispatch => {
    // dispatch(requestProperties(grip));

    // Use 'DomProvider' object exposed from the chrome scope.
    return DomProvider.getPrototypeAndProperties(grip).then(response => {
      dispatch(receiveProperties(grip, response));
    });
  };
}

// Exports from this module
exports.requestProperties = requestProperties;
exports.receiveProperties = receiveProperties;
exports.fetchProperties = fetchProperties;
