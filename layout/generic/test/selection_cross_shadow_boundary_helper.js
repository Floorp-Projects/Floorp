// Helper file for test_selection_cross_shadow_boundary_* related tests

function drag(fromTarget, fromX, fromY, toTarget, toX, toY) {
  synthesizeMouse(fromTarget, fromX, fromY, { type: "mousemove" });
  synthesizeMouse(fromTarget, fromX, fromY, { type: "mousedown" });
  synthesizeMouse(toTarget, toX, toY, { type: "mousemove" });
  synthesizeMouse(toTarget, toX, toY, { type: "mouseup" });
}
