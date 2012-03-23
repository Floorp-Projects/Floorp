/**
 * Tests if the given accessible at the given point is expected.
 *
 * @param aIdentifier        [in] accessible identifier
 * @param aX                 [in] x coordinate of the point relative accessible
 * @param aY                 [in] y coordinate of the point relative accessible
 * @param aFindDeepestChild  [in] points whether deepest or nearest child should
 *                           be returned
 * @param aChildIdentifier   [in] expected child accessible
 */
function testChildAtPoint(aIdentifier, aX, aY, aFindDeepestChild,
                          aChildIdentifier)
{
  var childAcc = getAccessible(aChildIdentifier);
  var actualChildAcc = getChildAtPoint(aIdentifier, aX, aY, aFindDeepestChild);

  var msg = "Wrong " + (aFindDeepestChild ? "deepest" : "direct");
  msg += " child accessible [" + prettyName(actualChildAcc);
  msg += "] at the point (" + aX + ", " + aY + ") of accessible [";
  msg += prettyName(aIdentifier) + "]";

  is(childAcc, actualChildAcc, msg);
}

/**
 * Test if getChildAtPoint returns the given child and grand child accessibles
 * at coordinates of child accessible (direct and deep hit test).
 */
function hitTest(aContainerID, aChildID, aGrandChildID)
{
  var container = getAccessible(aContainerID);
  var child = getAccessible(aChildID);
  var grandChild = getAccessible(aGrandChildID);

  var [x, y] = getBoundsForDOMElm(child);

  var actualChild = container.getChildAtPoint(x + 1, y + 1);
  is(actualChild, child,
     "Wrong child, expected: " + prettyName(child) +
     ", got: " + prettyName(actualChild));

  var actualGrandChild = container.getDeepestChildAtPoint(x + 1, y + 1);
  is(actualGrandChild, grandChild,
     "Wrong deepest child, expected: " + prettyName(grandChild) +
     ", got: " + prettyName(actualGrandChild));
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
function getChildAtPoint(aIdentifier, aX, aY, aFindDeepestChild)
{
  var acc = getAccessible(aIdentifier);
  if (!acc)
    return;

  var [screenX, screenY] = getBoundsForDOMElm(acc.DOMNode);

  var x = screenX + aX;
  var y = screenY + aY;

  try {
    if (aFindDeepestChild)
      return acc.getDeepestChildAtPoint(x, y);
    return acc.getChildAtPoint(x, y);
  } catch (e) {  }

  return null;
}

/**
 * Test the accessible boundaries.
 */
function testBounds(aID)
{
  var [expectedX, expectedY, expectedWidth, expectedHeight] =
    getBoundsForDOMElm(aID);

  var [x, y, width, height] = getBounds(aID);
  is(x, expectedX, "Wrong x coordinate of " + prettyName(aID));
  is(y, expectedY, "Wrong y coordinate of " + prettyName(aID));
  is(width, expectedWidth, "Wrong width of " + prettyName(aID));
  is(height, expectedHeight, "Wrong height of " + prettyName(aID));
}

/**
 * Return the accessible coordinates and size relative to the screen.
 */
function getBounds(aID)
{
  var accessible = getAccessible(aID);
  var x = {}, y = {}, width = {}, height = {};
  accessible.getBounds(x, y, width, height);
  return [x.value, y.value, width.value, height.value];
}

/**
 * Return DOM node coordinates relative the screen and its size in device
 * pixels.
 */
function getBoundsForDOMElm(aID)
{
  var x = 0, y = 0, width = 0, height = 0;

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

    var rect = img.getBoundingClientRect();
    x = rect.left + areaX;
    y = rect.top + areaY;
    width = areaWidth;
    height = areaHeight;
  }
  else {
    var rect = elm.getBoundingClientRect();
    x = rect.left;
    y = rect.top;
    width = rect.width;
    height = rect.height;
  }

  var elmWindow = elm.ownerDocument.defaultView;
  var winUtil = elmWindow.
    QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);

  var ratio = winUtil.screenPixelsPerCSSPixel;
  return [ (x + elmWindow.mozInnerScreenX) * ratio,
           (y + elmWindow.mozInnerScreenY) * ratio,
           width * ratio,
           height * ratio ];
}
