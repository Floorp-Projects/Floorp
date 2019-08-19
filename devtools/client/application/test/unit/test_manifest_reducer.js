/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  updateManifest,
} = require("devtools/client/application/src/actions/manifest.js");

const {
  manifestReducer,
  ManifestState,
} = require("devtools/client/application/src/reducers/manifest-state.js");

add_task(async function() {
  info("Test manifest reducer: UPDATE_MANIFEST action");
  const state = ManifestState();

  const action = updateManifest({ name: "foo" }, "some error");
  const newState = manifestReducer(state, action);
  equal(newState.manifest.name, "foo");
  equal(newState.errorMessage, "some error");
});
