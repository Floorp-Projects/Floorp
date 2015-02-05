/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets show the right player meta-data (name, duration,
// iteration count, delay).

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the simple animated node");
  yield selectNode(".animated", inspector);

  let titleEl = panel.playerWidgets[0].el.querySelector(".animation-title");
  ok(titleEl,
    "The player widget has a title element, where meta-data should be displayed");

  let nameEl = titleEl.querySelector("strong");
  ok(nameEl, "The first <strong> tag was retrieved, it should contain the name");
  is(nameEl.textContent, "simple-animation", "The animation name is correct");

  let metaDataEl = titleEl.querySelector(".meta-data");
  ok(metaDataEl, "The meta-data element exists");

  let metaDataEls = metaDataEl.querySelectorAll("strong");
  is(metaDataEls.length, 3, "3 meta-data elements were found");
  is(metaDataEls[0].textContent, "2s",
    "The first meta-data is the duration, and is correct");
  ok(!isNodeVisible(metaDataEls[1]),
    "The second meta-data is hidden, since there's no delay on the animation");

  info("Select the node with the delayed animation");
  yield selectNode(".delayed", inspector);

  titleEl = panel.playerWidgets[0].el.querySelector(".animation-title");
  nameEl = titleEl.querySelector("strong");
  is(nameEl.textContent, "simple-animation", "The animation name is correct");

  metaDataEls = titleEl.querySelectorAll(".meta-data strong");
  is(metaDataEls.length, 3,
    "3 meta-data elements were found for the delayed animation");
  is(metaDataEls[0].textContent, "3s",
    "The first meta-data is the duration, and is correct");
  ok(isNodeVisible(metaDataEls[0]), "The duration is shown");
  is(metaDataEls[1].textContent, "60s",
    "The second meta-data is the delay, and is correct");
  ok(isNodeVisible(metaDataEls[1]), "The delay is shown");
  is(metaDataEls[2].textContent, "10",
    "The third meta-data is the iteration count, and is correct");
  ok(isNodeVisible(metaDataEls[2]), "The iteration count is shown");
});
