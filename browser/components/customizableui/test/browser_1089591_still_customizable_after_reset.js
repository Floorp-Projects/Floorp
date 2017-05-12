"use strict";

// Dragging the elements again after a reset should work
add_task(async function() {
  await startCustomizing();
  let historyButton = document.getElementById("wrapper-history-panelmenu");
  let devButton = document.getElementById("wrapper-developer-button");

  ok(historyButton && devButton, "Draggable elements should exist");
  simulateItemDrag(historyButton, devButton);
  await gCustomizeMode.reset();
  ok(CustomizableUI.inDefaultState, "Should be back in default state");

  historyButton = document.getElementById("wrapper-history-panelmenu");
  devButton = document.getElementById("wrapper-developer-button");
  ok(historyButton && devButton, "Draggable elements should exist");
  simulateItemDrag(historyButton, devButton);

  await endCustomizing();
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
