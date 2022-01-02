/* import-globals-from common.js */

/**
 * Tests if the given child and grand child accessibles at the given point are
 * expected.
 *
 * @param aID            [in] accessible identifier
 * @param aX             [in] x coordinate of the point relative accessible
 * @param aY             [in] y coordinate of the point relative accessible
 * @param aChildID       [in] expected child accessible
 * @param aGrandChildID  [in] expected child accessible
 */
function testChildAtPoint(aID, aX, aY, aChildID, aGrandChildID) {
  var child = getChildAtPoint(aID, aX, aY, false);
  var expectedChild = getAccessible(aChildID);

  var msg =
    "Wrong direct child accessible at the point (" +
    aX +
    ", " +
    aY +
    ") of " +
    prettyName(aID);
  isObject(child, expectedChild, msg);

  var grandChild = getChildAtPoint(aID, aX, aY, true);
  var expectedGrandChild = getAccessible(aGrandChildID);

  msg =
    "Wrong deepest child accessible at the point (" +
    aX +
    ", " +
    aY +
    ") of " +
    prettyName(aID);
  isObject(grandChild, expectedGrandChild, msg);
}

/**
 * Test if getChildAtPoint returns the given child and grand child accessibles
 * at coordinates of child accessible (direct and deep hit test).
 */
function hitTest(aContainerID, aChildID, aGrandChildID) {
  var container = getAccessible(aContainerID);
  var child = getAccessible(aChildID);
  var grandChild = getAccessible(aGrandChildID);

  var [x, y] = getBoundsForDOMElm(child);

  var actualChild = container.getChildAtPoint(x + 1, y + 1);
  isObject(
    actualChild,
    child,
    "Wrong direct child of " + prettyName(aContainerID)
  );

  var actualGrandChild = container.getDeepestChildAtPoint(x + 1, y + 1);
  isObject(
    actualGrandChild,
    grandChild,
    "Wrong deepest child of " + prettyName(aContainerID)
  );
}

/**
 * Test if getOffsetAtPoint returns the given text offset at given coordinates.
 */
function testOffsetAtPoint(aHyperTextID, aX, aY, aCoordType, aExpectedOffset) {
  var hyperText = getAccessible(aHyperTextID, [nsIAccessibleText]);
  var offset = hyperText.getOffsetAtPoint(aX, aY, aCoordType);
  is(
    offset,
    aExpectedOffset,
    "Wrong offset at given point (" +
      aX +
      ", " +
      aY +
      ") for " +
      prettyName(aHyperTextID)
  );
}

/**
 * Zoom the given document.
 */
function zoomDocument(aDocument, aZoom) {
  SpecialPowers.setFullZoom(aDocument.defaultView, aZoom);
}

/**
 * Set the relative resolution of this document. This is what apz does.
 * On non-mobile platforms you won't see a visible change.
 */
function setResolution(aDocument, aZoom) {
  var windowUtils = aDocument.defaultView.windowUtils;

  windowUtils.setResolutionAndScaleTo(aZoom);
}

/**
 * Return child accessible at the given point.
 *
 * @param aIdentifier        [in] accessible identifier
 * @param aX                 [in] x coordinate of the point relative accessible
 * @param aY                 [in] y coordinate of the point relative accessible
 * @param aFindDeepestChild  [in] points whether deepest or nearest child should
 *                           be returned
 * @return                   the child accessible at the given point
 */
function getChildAtPoint(aIdentifier, aX, aY, aFindDeepestChild) {
  var acc = getAccessible(aIdentifier);
  if (!acc) {
    return null;
  }

  var [screenX, screenY] = getBoundsForDOMElm(acc.DOMNode);

  var x = screenX + aX;
  var y = screenY + aY;

  try {
    if (aFindDeepestChild) {
      return acc.getDeepestChildAtPoint(x, y);
    }
    return acc.getChildAtPoint(x, y);
  } catch (e) {}

  return null;
}

/**
 * Test the accessible position.
 */
function testPos(aID, aPoint) {
  var [expectedX, expectedY] =
    aPoint != undefined ? aPoint : getBoundsForDOMElm(aID);

  var [x, y] = getBounds(aID);
  is(x, expectedX, "Wrong x coordinate of " + prettyName(aID));
  is(y, expectedY, "Wrong y coordinate of " + prettyName(aID));
}

/**
 * Test the accessible boundaries.
 */
function testBounds(aID, aRect) {
  var [expectedX, expectedY, expectedWidth, expectedHeight] =
    aRect != undefined ? aRect : getBoundsForDOMElm(aID);

  var [x, y, width, height] = getBounds(aID);
  is(x, expectedX, "Wrong x coordinate of " + prettyName(aID));
  is(y, expectedY, "Wrong y coordinate of " + prettyName(aID));
  is(width, expectedWidth, "Wrong width of " + prettyName(aID));
  is(height, expectedHeight, "Wrong height of " + prettyName(aID));
}

/**
 * Test text position at the given offset.
 */
function testTextPos(aID, aOffset, aPoint, aCoordOrigin) {
  var [expectedX, expectedY] = aPoint;

  var xObj = {},
    yObj = {};
  var hyperText = getAccessible(aID, [nsIAccessibleText]);
  hyperText.getCharacterExtents(aOffset, xObj, yObj, {}, {}, aCoordOrigin);
  is(
    xObj.value,
    expectedX,
    "Wrong x coordinate at offset " + aOffset + " for " + prettyName(aID)
  );
  ok(
    yObj.value - expectedY < 2 && expectedY - yObj.value < 2,
    "Wrong y coordinate at offset " +
      aOffset +
      " for " +
      prettyName(aID) +
      " - got " +
      yObj.value +
      ", expected " +
      expectedY +
      "The difference doesn't exceed 1."
  );
}

/**
 * Test text bounds that is enclosed betwene the given offsets.
 */
function testTextBounds(aID, aStartOffset, aEndOffset, aRect, aCoordOrigin) {
  var [expectedX, expectedY, expectedWidth, expectedHeight] = aRect;

  var xObj = {},
    yObj = {},
    widthObj = {},
    heightObj = {};
  var hyperText = getAccessible(aID, [nsIAccessibleText]);
  hyperText.getRangeExtents(
    aStartOffset,
    aEndOffset,
    xObj,
    yObj,
    widthObj,
    heightObj,
    aCoordOrigin
  );

  // x
  is(
    xObj.value,
    expectedX,
    "Wrong x coordinate of text between offsets (" +
      aStartOffset +
      ", " +
      aEndOffset +
      ") for " +
      prettyName(aID)
  );

  // y
  isWithin(
    yObj.value,
    expectedY,
    1,
    `y coord of text between offsets (${aStartOffset}, ${aEndOffset}) ` +
      `for ${prettyName(aID)}`
  );

  // Width
  var msg =
    "Wrong width of text between offsets (" +
    aStartOffset +
    ", " +
    aEndOffset +
    ") for " +
    prettyName(aID);
  if (widthObj.value == expectedWidth) {
    ok(true, msg);
  } else {
    todo(false, msg);
  } // fails on some windows machines

  // Height
  isWithin(
    heightObj.value,
    expectedHeight,
    1,
    `height of text between offsets (${aStartOffset}, ${aEndOffset}) ` +
      `for ${prettyName(aID)}`
  );
}

/**
 * Return the accessible coordinates relative to the screen in device pixels.
 */
function getPos(aID) {
  var accessible = getAccessible(aID);
  var x = {},
    y = {};
  accessible.getBounds(x, y, {}, {});
  return [x.value, y.value];
}

/**
 * Return the accessible coordinates and size relative to the screen in device
 * pixels. This methods also retrieves coordinates in CSS pixels and ensures that they
 * match Dev pixels with a given device pixel ratio.
 */
function getBounds(aID, aDPR = window.devicePixelRatio) {
  const accessible = getAccessible(aID);
  let x = {},
    y = {},
    width = {},
    height = {};
  let xInCSS = {},
    yInCSS = {},
    widthInCSS = {},
    heightInCSS = {};
  accessible.getBounds(x, y, width, height);
  accessible.getBoundsInCSSPixels(xInCSS, yInCSS, widthInCSS, heightInCSS);

  info(`DPR is: ${aDPR}`);
  isWithin(
    xInCSS.value,
    x.value / aDPR,
    1,
    "X in CSS pixels is calculated correctly"
  );
  isWithin(
    yInCSS.value,
    y.value / aDPR,
    1,
    "Y in CSS pixels is calculated correctly"
  );
  isWithin(
    widthInCSS.value,
    width.value / aDPR,
    1,
    "Width in CSS pixels is calculated correctly"
  );
  isWithin(
    heightInCSS.value,
    height.value / aDPR,
    1,
    "Height in CSS pixels is calculated correctly"
  );

  return [x.value, y.value, width.value, height.value];
}

function getRangeExtents(aID, aStartOffset, aEndOffset, aCoordOrigin) {
  var hyperText = getAccessible(aID, [nsIAccessibleText]);
  var x = {},
    y = {},
    width = {},
    height = {};
  hyperText.getRangeExtents(
    aStartOffset,
    aEndOffset,
    x,
    y,
    width,
    height,
    aCoordOrigin
  );
  return [x.value, y.value, width.value, height.value];
}

/**
 * Return DOM node coordinates relative the screen and its size in device
 * pixels.
 */
function getBoundsForDOMElm(aID) {
  var x = 0,
    y = 0,
    width = 0,
    height = 0;

  var elm = getNode(aID);
  if (elm.localName == "area") {
    var mapName = elm.parentNode.getAttribute("name");
    var selector = "[usemap='#" + mapName + "']";
    var img = elm.ownerDocument.querySelector(selector);

    var areaCoords = elm.coords.split(",");
    var areaX = parseInt(areaCoords[0]);
    var areaY = parseInt(areaCoords[1]);
    var areaWidth = parseInt(areaCoords[2]) - areaX;
    var areaHeight = parseInt(areaCoords[3]) - areaY;

    let rect = img.getBoundingClientRect();
    x = rect.left + areaX;
    y = rect.top + areaY;
    width = areaWidth;
    height = areaHeight;
  } else {
    let rect = elm.getBoundingClientRect();
    x = rect.left;
    y = rect.top;
    width = rect.width;
    height = rect.height;
  }

  var elmWindow = elm.ownerGlobal;
  return CSSToDevicePixels(
    elmWindow,
    x + elmWindow.mozInnerScreenX,
    y + elmWindow.mozInnerScreenY,
    width,
    height
  );
}

function CSSToDevicePixels(aWindow, aX, aY, aWidth, aHeight) {
  var winUtil = aWindow.windowUtils;

  var ratio = winUtil.screenPixelsPerCSSPixel;

  // CSS pixels and ratio can be not integer. Device pixels are always integer.
  // Do our best and hope it works.
  return [
    Math.round(aX * ratio),
    Math.round(aY * ratio),
    Math.round(aWidth * ratio),
    Math.round(aHeight * ratio),
  ];
}
