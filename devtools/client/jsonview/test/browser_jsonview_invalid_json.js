/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "invalid_json.json";

add_task(async function () {
  info("Test invalid JSON started");

  await addJsonViewTab(TEST_JSON_URL);

  const count = await getElementCount(".textPanelBox");
  ok(count == 1, "The raw data panel must be shown");

  const text = await getElementText(".textPanelBox .jsonParseError");
  ok(text, "There must be an error description");
});
