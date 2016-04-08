/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that a page navigation resets the state of the global toggle button.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the non-animated test node");
  yield selectNodeAndWaitForAnimations(".still", inspector);

  ok(!panel.toggleAllButtonEl.classList.contains("paused"),
    "The toggle button is in its running state by default");

  info("Toggle all animations, so that they pause");
  yield panel.toggleAll();
  ok(panel.toggleAllButtonEl.classList.contains("paused"),
    "The toggle button now is in its paused state");

  info("Reloading the page");
  yield reloadTab(inspector);

  ok(!panel.toggleAllButtonEl.classList.contains("paused"),
    "The toggle button is back in its running state");
});
