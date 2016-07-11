/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests deleting presets

const {CSSFilterEditorWidget} = require("devtools/client/shared/widgets/FilterWidget");

const TEST_URI = `data:text/html,<div id="filter-container" />`;

add_task(function* () {
  let [host, win, doc] = yield createHost("bottom", TEST_URI);

  const container = doc.querySelector("#filter-container");
  let widget = new CSSFilterEditorWidget(container, "none");
  // First render
  yield widget.once("render");

  const NAME = "Test";
  const VALUE = "blur(2px) contrast(150%)";

  yield showFilterPopupPresetsAndCreatePreset(widget, NAME, VALUE);

  let removeButton = widget.el.querySelector(".preset .remove-button");
  let onRender = widget.once("render");
  widget._presetClick({
    target: removeButton
  });

  yield onRender;
  is(widget.el.querySelector(".preset"), null,
     "Should re-render after removing preset");

  let list = yield widget.getPresets();
  is(list.length, 0,
     "Should remove presets from asyncStorage");
});
