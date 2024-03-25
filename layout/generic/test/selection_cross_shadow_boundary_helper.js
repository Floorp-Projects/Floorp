// Helper file for test_selection_cross_shadow_boundary_* related tests

function drag(
  fromTarget,
  fromX,
  fromY,
  toTarget,
  toX,
  toY,
  withAccelKey = false
) {
  synthesizeMouse(fromTarget, fromX, fromY, {
    type: "mousemove",
    accelKey: withAccelKey,
  });
  synthesizeMouse(fromTarget, fromX, fromY, {
    type: "mousedown",
    accelKey: withAccelKey,
  });
  synthesizeMouse(toTarget, toX, toY, {
    type: "mousemove",
    accelKey: withAccelKey,
  });
  synthesizeMouse(toTarget, toX, toY, {
    type: "mouseup",
    accelKey: withAccelKey,
  });
}
