/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the CubicBezierPresetWidget cycles menus

const TEST_URI = "chrome://devtools/content/shared/widgets/cubic-bezier-frame.xhtml";
const {CubicBezierPresetWidget} =
  require("devtools/client/shared/widgets/CubicBezierWidget");
const {PREDEFINED, PRESETS, DEFAULT_PRESET_CATEGORY} =
  require("devtools/client/shared/widgets/CubicBezierPresets");

add_task(function*() {
  yield promiseTab("about:blank");
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  let container = doc.querySelector("#container");
  let w = new CubicBezierPresetWidget(container);

  info("Checking that preset is selected if coordinates are known");

  w.refreshMenu([0, 0, 0, 0]);
  is(w.activeCategory, container.querySelector(`#${DEFAULT_PRESET_CATEGORY}`),
    "The default category is selected");
  is(w._activePreset, null, "There is no selected category");

  w.refreshMenu(PREDEFINED["linear"]);
  is(w.activeCategory, container.querySelector("#ease-in-out"),
     "The ease-in-out category is active");
  is(w._activePreset, container.querySelector("#ease-in-out-linear"),
     "The ease-in-out-linear preset is active");

  w.refreshMenu(PRESETS["ease-out"]["ease-out-sine"]);
  is(w.activeCategory, container.querySelector("#ease-out"),
     "The ease-out category is active");
  is(w._activePreset, container.querySelector("#ease-out-sine"),
     "The ease-out-sine preset is active");

  w.refreshMenu([0, 0, 0, 0]);
  is(w.activeCategory, container.querySelector("#ease-out"),
     "The ease-out category is still active");
  is(w._activePreset, null, "No preset is active");

  w.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});
