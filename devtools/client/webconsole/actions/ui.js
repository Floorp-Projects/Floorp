/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllUi } = require("devtools/client/webconsole/selectors/ui");
const { getMessage } = require("devtools/client/webconsole/selectors/messages");

const {
  FILTER_BAR_TOGGLE,
  INITIALIZE,
  PERSIST_TOGGLE,
  PREFS,
  SELECT_NETWORK_MESSAGE_TAB,
  SIDEBAR_CLOSE,
  SHOW_OBJECT_IN_SIDEBAR,
  TIMESTAMPS_TOGGLE,
  SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
} = require("devtools/client/webconsole/constants");

function filterBarToggle(show) {
  return (dispatch, getState, {prefsService}) => {
    dispatch({
      type: FILTER_BAR_TOGGLE,
    });
    const {filterBarVisible} = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.FILTER_BAR, filterBarVisible);
  };
}

function persistToggle(show) {
  return (dispatch, getState, {prefsService}) => {
    dispatch({
      type: PERSIST_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.PERSIST, uiState.persistLogs);
  };
}

function timestampsToggle(visible) {
  return {
    type: TIMESTAMPS_TOGGLE,
    visible,
  };
}

function selectNetworkMessageTab(id) {
  return {
    type: SELECT_NETWORK_MESSAGE_TAB,
    id,
  };
}

function initialize() {
  return {
    type: INITIALIZE
  };
}

function sidebarClose(show) {
  return {
    type: SIDEBAR_CLOSE,
  };
}

function splitConsoleCloseButtonToggle(shouldDisplayButton) {
  return {
    type: SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
    shouldDisplayButton,
  };
}

function showObjectInSidebar(actorId, messageId) {
  return (dispatch, getState) => {
    let { parameters } = getMessage(getState(), messageId);
    if (Array.isArray(parameters)) {
      for (let parameter of parameters) {
        if (parameter.actor === actorId) {
          dispatch({
            type: SHOW_OBJECT_IN_SIDEBAR,
            grip: parameter
          });
          return;
        }
      }
    }
  };
}

module.exports = {
  filterBarToggle,
  initialize,
  persistToggle,
  selectNetworkMessageTab,
  sidebarClose,
  showObjectInSidebar,
  timestampsToggle,
  splitConsoleCloseButtonToggle,
};
