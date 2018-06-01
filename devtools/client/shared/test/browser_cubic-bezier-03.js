/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that coordinates can be changed programatically in the CubicBezierWidget

const {CubicBezierWidget} =
  require("devtools/client/shared/widgets/CubicBezierWidget");
const {PREDEFINED} = require("devtools/client/shared/widgets/CubicBezierPresets");

const TEST_URI = CHROME_URL_ROOT + "doc_cubic-bezier-01.html";

add_task(async function() {
  const [host,, doc] = await createHost("bottom", TEST_URI);

  const container = doc.querySelector("#cubic-bezier-container");
  const w = new CubicBezierWidget(container, PREDEFINED.linear);

  await coordinatesCanBeChangedByProvidingAnArray(w);
  await coordinatesCanBeChangedByProvidingAValue(w);

  w.destroy();
  host.destroy();
});

async function coordinatesCanBeChangedByProvidingAnArray(widget) {
  info("Listening for the update event");
  const onUpdated = widget.once("updated");

  info("Setting new coordinates");
  widget.coordinates = [0, 1, 1, 0];

  const bezier = await onUpdated;
  ok(true, "The updated event was fired as a result of setting coordinates");

  is(bezier.P1[0], 0, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 1, "The new P1 progress coordinate is correct");
  is(bezier.P2[0], 1, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 0, "The new P2 progress coordinate is correct");
}

async function coordinatesCanBeChangedByProvidingAValue(widget) {
  info("Listening for the update event");
  let onUpdated = widget.once("updated");

  info("Setting linear css value");
  widget.cssCubicBezierValue = "linear";
  let bezier = await onUpdated;
  ok(true, "The updated event was fired as a result of setting cssValue");

  is(bezier.P1[0], 0, "The new P1 time coordinate is correct");
  is(bezier.P1[1], 0, "The new P1 progress coordinate is correct");
  is(bezier.P2[0], 1, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 1, "The new P2 progress coordinate is correct");

  info("Setting a custom cubic-bezier css value");
  onUpdated = widget.once("updated");
  widget.cssCubicBezierValue = "cubic-bezier(.25,-0.5, 1, 1.25)";
  bezier = await onUpdated;
  ok(true, "The updated event was fired as a result of setting cssValue");

  is(bezier.P1[0], .25, "The new P1 time coordinate is correct");
  is(bezier.P1[1], -.5, "The new P1 progress coordinate is correct");
  is(bezier.P2[0], 1, "The new P2 time coordinate is correct");
  is(bezier.P2[1], 1.25, "The new P2 progress coordinate is correct");
}
