/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  updateSelectedPage,
} = require("devtools/client/application/src/actions/ui.js");

const {
  uiReducer,
  UiState,
} = require("devtools/client/application/src/reducers/ui-state.js");

add_task(async function() {
  info("Test ui reducer: UPDATE_SELECTED_PAGE action");
  const state = UiState();
  const action = updateSelectedPage("foo");

  const newState = uiReducer(state, action);
  equal(newState.selectedPage, "foo");
});
