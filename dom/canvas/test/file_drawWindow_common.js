const CANVAS_WIDTH = 200;
const CANVAS_HEIGHT = 100;

function runDrawWindowTests(win, drawWindowFlags, transparentBackground) {

  function make_canvas() {
    var canvas = document.createElement("canvas");
    canvas.setAttribute("height", CANVAS_HEIGHT);
    canvas.setAttribute("width", CANVAS_WIDTH);
    document.body.appendChild(canvas);
    return canvas;
  }

  var testCanvas = make_canvas();
  var refCanvas = make_canvas();

  var testCx = testCanvas.getContext("2d");
  var refCx = refCanvas.getContext("2d");

  var testWrapCx = SpecialPowers.wrap(testCx);
  var refWrapCx = SpecialPowers.wrap(refCx);

  function clearRef(fillStyle) {
    refCx.setTransform(1, 0, 0, 1, 0, 0);
    refCx.fillStyle = fillStyle;
    refCx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
  }
  function clearTest(fillStyle) {
    testCx.setTransform(1, 0, 0, 1, 0, 0);
    testCx.fillStyle = fillStyle;
    testCx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
  }
  function clear(fillStyle) {
    clearRef(fillStyle);
    clearTest(fillStyle);
  }

  // Basic tests of drawing the whole document on a background

  clear("white");
  testWrapCx.drawWindow(win, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                        "rgb(255, 255, 255)", drawWindowFlags);
  refCx.fillStyle = "fuchsia";
  refCx.fillRect(10, 10, 20, 20);
  refCx.fillStyle = "aqua";
  refCx.fillRect(50, 10, 20, 20);
  refCx.fillStyle = "yellow";
  refCx.fillRect(90, 10, 20, 20);
  assertSnapshots(testCanvas, refCanvas, true /* equal */, null /*no fuzz*/,
                  "full draw of source on white background", "reference");

  clearTest("white");
  testWrapCx.drawWindow(win, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                        "rgb(255, 255, 0)", drawWindowFlags);
  assertSnapshots(testCanvas, refCanvas,
                  !transparentBackground /* not equal */, null /*no fuzz*/,
                  "full draw of source on yellow background", "reference");

  clearRef("yellow");
  refCx.fillStyle = "fuchsia";
  refCx.fillRect(10, 10, 20, 20);
  refCx.fillStyle = "aqua";
  refCx.fillRect(50, 10, 20, 20);
  refCx.fillStyle = "yellow";
  refCx.fillRect(90, 10, 20, 20);

  assertSnapshots(testCanvas, refCanvas, transparentBackground /* equal */,
                  null /*no fuzz*/,
                  "full draw of source on yellow background", "reference");

  // Test drawing a region within the document.

  clear("white");

  testCx.translate(17, 31);
  testWrapCx.drawWindow(win, 40, 0, 40, 40,
                        "white", drawWindowFlags);

  refCx.fillStyle = "aqua";
  refCx.fillRect(17 + 10, 31 + 10, 20, 20);

  assertSnapshots(testCanvas, refCanvas, true /* equal */, null /*no fuzz*/,
                  "draw of subrect of source with matching background",
                  "reference");

  clear("blue");

  testCx.translate(17, 31);
  testWrapCx.drawWindow(win, 40, 0, 35, 45,
                        "green", drawWindowFlags);

  if (transparentBackground) {
    refCx.fillStyle = "green";
  } else {
    refCx.fillStyle = "white";
  }
  refCx.fillRect(17, 31, 35, 45);
  refCx.fillStyle = "aqua";
  refCx.fillRect(17 + 10, 31 + 10, 20, 20);

  assertSnapshots(testCanvas, refCanvas, true /* equal */, null /*no fuzz*/,
                  "draw of subrect of source with different background",
                  "reference");

  // Test transparency of background not disturbing what is behind
  clear("blue");

  testCx.translate(17, 31);
  testWrapCx.drawWindow(win, 40, 0, 35, 45,
                        "transparent", drawWindowFlags);

  if (!transparentBackground) {
    refCx.fillStyle = "white";
    refCx.fillRect(17, 31, 35, 45);
  }
  refCx.fillStyle = "aqua";
  refCx.fillRect(17 + 10, 31 + 10, 20, 20);

  assertSnapshots(testCanvas, refCanvas, true /* equal */, null /*no fuzz*/,
                  "draw of subrect of source with different background",
                  "reference");

  // Test that multiple drawWindow calls draw at correct positions.
  clear("blue");

  testCx.translate(9, 3);
  // 5, 8 is 5, 2 from the corner of the fuchsia square
  testWrapCx.drawWindow(win, 5, 8, 30, 25,
                        "maroon", drawWindowFlags);
  // 35, 0 is 15, 10 from the corner of the aqua square
  testWrapCx.drawWindow(win, 35, 0, 50, 40,
                        "transparent", drawWindowFlags);
  testCx.translate(15, 0);
  // 85, 5 is 5, 5 from the corner of the yellow square
  testWrapCx.drawWindow(win, 85, 5, 30, 25,
                        "transparent", drawWindowFlags);

  if (transparentBackground) {
    refCx.fillStyle = "maroon";
    refCx.fillRect(9, 3, 30, 25);
    refCx.fillStyle = "fuchsia";
    refCx.fillRect(9 + 5, 3 + 2, 20, 20);
  } else {
    refCx.fillStyle = "white";
    refCx.fillRect(9, 3, 50, 40);
  }
  refCx.fillStyle = "aqua";
  refCx.fillRect(9 + 15, 3 + 10, 20, 20);
  if (!transparentBackground) {
    refCx.fillStyle = "white";
    refCx.fillRect(9 + 15, 3, 30, 25);
  }
  refCx.fillStyle = "yellow";
  refCx.fillRect(9 + 15 + 5, 3 + 0 + 5, 20, 20);

  assertSnapshots(testCanvas, refCanvas, true /* equal */, null /*no fuzz*/,
                  "multiple drawWindow calls on top of each other",
                  "reference");
}
