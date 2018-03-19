/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the spacebar key press toggles the toggleAll button state
// when a node with no animation is selected.
// This test doesn't need to test if animations actually pause/resume
// because there's an other test that does this :
// browser_animation_toggle_button_toggles_animation.js

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel, inspector, window, controller} = await openAnimationInspector();
  let {toggleAllButtonEl} = panel;

  // select a node without animations
  await selectNodeAndWaitForAnimations(".still", inspector);

  // ensure the focus is on the animation panel
  window.focus();

  info("Simulate spacebar stroke and check toggleAll button" +
       " is in paused state");

  // sending the key will lead to a ALL_ANIMATIONS_TOGGLED_EVENT
  let onToggled = once(controller, controller.ALL_ANIMATIONS_TOGGLED_EVENT);
  EventUtils.sendKey("SPACE", window);
  await onToggled;
  ok(toggleAllButtonEl.classList.contains("paused"),
   "The toggle all button is in its paused state");

  info("Simulate spacebar stroke and check toggleAll button" +
       " is in playing state");

  // sending the key will lead to a ALL_ANIMATIONS_TOGGLED_EVENT
  onToggled = once(controller, controller.ALL_ANIMATIONS_TOGGLED_EVENT);
  EventUtils.sendKey("SPACE", window);
  await onToggled;
  ok(!toggleAllButtonEl.classList.contains("paused"),
   "The toggle all button is in its playing state again");
});
