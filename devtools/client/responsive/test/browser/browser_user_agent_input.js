/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";
const NEW_USER_AGENT = "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0";

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnUAChange(true);

  await waitUntilState(store, state => state.viewports.length == 1);

  info("Check the default state of the user agent input");
  await testUserAgent(ui, DEFAULT_UA);

  info(`Change the user agent input to ${NEW_USER_AGENT}`);
  await changeUserAgentInput(ui, NEW_USER_AGENT);
  await testUserAgent(ui, NEW_USER_AGENT);

  info("Reset the user agent input back to the default UA");
  await changeUserAgentInput(ui, "");
  await testUserAgent(ui, DEFAULT_UA);

  reloadOnUAChange(false);
});
