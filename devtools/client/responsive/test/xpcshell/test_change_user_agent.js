/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test changing the user agent.

const { changeUserAgent } = require("devtools/client/responsive/actions/ui");

const NEW_USER_AGENT =
  "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_6) " +
  "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36";

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  equal(getState().ui.userAgent, "", "User agent is empty by default.");

  dispatch(changeUserAgent(NEW_USER_AGENT));
  equal(
    getState().ui.userAgent,
    NEW_USER_AGENT,
    `User Agent changed to ${NEW_USER_AGENT}`
  );
});
