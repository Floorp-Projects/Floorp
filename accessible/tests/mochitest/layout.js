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
  var elm = getNode(aID);
  var elmWindow = elm.ownerDocument.defaultView;
  var winUtil = elmWindow.
    QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);

  var ratio = winUtil.screenPixelsPerCSSPixel;
  var rect = elm.getBoundingClientRect();
  return [ (rect.left + elmWindow.mozInnerScreenX) * ratio,
           (rect.top + elmWindow.mozInnerScreenY) * ratio,
           rect.width * ratio,
           rect.height * ratio ];
}
