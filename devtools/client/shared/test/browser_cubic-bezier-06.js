
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the integration between CubicBezierWidget and CubicBezierPresets

const {CubicBezierWidget} =
  require("devtools/client/shared/widgets/CubicBezierWidget");
const {PRESETS} = require("devtools/client/shared/widgets/CubicBezierPresets");

const TEST_URI = CHROME_URL_ROOT + "doc_cubic-bezier-01.html";

add_task(async function() {
  const [host, win, doc] = await createHost("bottom", TEST_URI);

  const container = doc.querySelector("#cubic-bezier-container");
  const w = new CubicBezierWidget(container,
              PRESETS["ease-in"]["ease-in-sine"]);
  w.presets.refreshMenu(PRESETS["ease-in"]["ease-in-sine"]);

  const rect = w.curve.getBoundingClientRect();
  rect.graphTop = rect.height * w.bezierCanvas.padding[0];

  await adjustingBezierUpdatesPreset(w, win, doc, rect);
  await selectingPresetUpdatesBezier(w, win, doc, rect);

  w.destroy();
  host.destroy();
});

function adjustingBezierUpdatesPreset(widget, win, doc, rect) {
  info("Checking that changing the bezier refreshes the preset menu");

  is(widget.presets.activeCategory,
     doc.querySelector("#ease-in"),
     "The selected category is ease-in");

  is(widget.presets._activePreset,
     doc.querySelector("#ease-in-sine"),
     "The selected preset is ease-in-sine");

  info("Generating custom bezier curve by dragging");
  widget._onPointMouseDown({target: widget.p1});
  doc.onmousemove({pageX: rect.left, pageY: rect.graphTop});
  doc.onmouseup();

  is(widget.presets.activeCategory,
     doc.querySelector("#ease-in"),
     "The selected category is still ease-in");

  is(widget.presets._activePreset, null,
     "There is no active preset");
}

async function selectingPresetUpdatesBezier(widget, win, doc, rect) {
  info("Checking that selecting a preset updates bezier curve");

  info("Listening for the new coordinates event");
  const onNewCoordinates = widget.presets.once("new-coordinates");
  const onUpdated = widget.once("updated");

  info("Click a preset");
  const preset = doc.querySelector("#ease-in-sine");
  widget.presets._onPresetClick({currentTarget: preset});

  await onNewCoordinates;
  ok(true, "The preset widget fired the new-coordinates event");

  const bezier = await onUpdated;
  ok(true, "The bezier canvas fired the updated event");

  is(bezier.P1[0], preset.coordinates[0], "The new P1 time coordinate is correct");
  is(bezier.P1[1], preset.coordinates[1], "The new P1 progress coordinate is correct");
  is(bezier.P2[0], preset.coordinates[2], "P2 time coordinate is correct ");
  is(bezier.P2[1], preset.coordinates[3], "P2 progress coordinate is correct");
}
