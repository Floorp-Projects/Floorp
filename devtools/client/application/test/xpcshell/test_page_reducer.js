/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  updateDomain,
} = require("resource://devtools/client/application/src/actions/page.js");

const {
  pageReducer,
  PageState,
} = require("resource://devtools/client/application/src/reducers/page-state.js");

add_task(async function () {
  info("Test page reducer: UPDATE_DOMAIN action");
  const state = PageState();
  const action = updateDomain("https://example.com/foo/#bar");

  const newState = pageReducer(state, action);
  equal(newState.domain, "example.com");
});
