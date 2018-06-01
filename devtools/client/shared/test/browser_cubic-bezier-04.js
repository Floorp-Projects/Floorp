/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the CubicBezierPresetWidget generates markup.

const {CubicBezierPresetWidget} =
  require("devtools/client/shared/widgets/CubicBezierWidget");
const {PRESETS} = require("devtools/client/shared/widgets/CubicBezierPresets");

const TEST_URI = CHROME_URL_ROOT + "doc_cubic-bezier-01.html";

add_task(async function() {
  const [host,, doc] = await createHost("bottom", TEST_URI);

  const container = doc.querySelector("#cubic-bezier-container");
  const w = new CubicBezierPresetWidget(container);

  info("Checking that the presets are created in the parent");
  ok(container.querySelector(".preset-pane"),
     "The preset pane has been added");

  ok(container.querySelector("#preset-categories"),
     "The preset categories have been added");
  const categories = container.querySelectorAll(".category");
  is(categories.length, Object.keys(PRESETS).length,
     "The preset categories have been added");
  Object.keys(PRESETS).forEach(category => {
    ok(container.querySelector("#" + category), `${category} has been added`);
    ok(container.querySelector("#preset-category-" + category),
       `The preset list for ${category} has been added.`);
  });

  info("Checking that each of the presets and its preview have been added");
  Object.keys(PRESETS).forEach(category => {
    Object.keys(PRESETS[category]).forEach(presetLabel => {
      const preset = container.querySelector("#" + presetLabel);
      ok(preset, `${presetLabel} has been added`);
      ok(preset.querySelector("canvas"),
         `${presetLabel}'s canvas preview has been added`);
      ok(preset.querySelector("p"),
         `${presetLabel}'s label has been added`);
    });
  });

  w.destroy();
  host.destroy();
});
