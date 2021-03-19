/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_homepage_button_with_homepage() {
  let homeButton = window.document.getElementById("home-button");
  isnot(homeButton, null, "The home button should be visible");
});
