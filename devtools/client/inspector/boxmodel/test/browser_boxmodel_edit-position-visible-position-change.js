/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the 'Edit Position' button is still visible after
// layout is changed.
// see bug 1398722

const TEST_URI = `
  <div id="mydiv" style="background:tomato;
    position:absolute;
    top:10px;
    left:10px;
    width:100px;
    height:100px">
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, boxmodel} = await openLayoutView();

  await selectNode("#mydiv", inspector);
  let editPositionButton = boxmodel.document.querySelector(".layout-geometry-editor");

  ok(isNodeVisible(editPositionButton), "Edit Position button is visible initially");

  const positionLeftTextbox = boxmodel.document.querySelector(
      ".boxmodel-editable[title=position-left]"
  );
  ok(isNodeVisible(positionLeftTextbox), "Position-left edit box exists");

  info("Change the value of position-left and submit");
  const onUpdate = waitForUpdate(inspector);
  EventUtils.synthesizeMouseAtCenter(positionLeftTextbox, {},
    boxmodel.document.defaultView);
  EventUtils.synthesizeKey("8", {}, boxmodel.document.defaultView);
  EventUtils.synthesizeKey("VK_RETURN", {}, boxmodel.document.defaultView);

  await onUpdate;
  editPositionButton = boxmodel.document.querySelector(".layout-geometry-editor");
  ok(isNodeVisible(editPositionButton),
    "Edit Position button is still visible after layout change");
});
