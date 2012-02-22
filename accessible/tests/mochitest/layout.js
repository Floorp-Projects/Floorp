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
  var nodeObj = { value: null };
  var acc = getAccessible(aIdentifier, null, nodeObj);
  var node = nodeObj.value;

  if (!acc || !node)
    return;

  var [deltaX, deltaY] = getScreenCoords(node);

  var x = deltaX + aX;
  var y = deltaY + aY;

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
function testBounds(aID, aX, aY, aWidth, aHeight)
{
  var [x, y, width, height] = getBounds(aID);
  is(x, aX, "Wrong x coordinate of " + prettyName(aID));
  is(y, aY, "Wrong y coordinate of " + prettyName(aID));
  // XXX: width varies depending on platform
  //is(width, aWidth, "Wrong width of " + prettyName(aID));
  is(height, aHeight, "Wrong height of " + prettyName(aID));
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
 * Return DOM node coordinates relative screen as pair (x, y).
 */
function getScreenCoords(aNode)
{
  if (aNode instanceof nsIDOMXULElement)
    return [node.boxObject.screenX, node.boxObject.screenY];

  // Ugly hack until bug 486200 is fixed.
  const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var descr = document.createElementNS(XUL_NS, "description");
  descr.setAttribute("value", "helper description");
  aNode.parentNode.appendChild(descr);
  var descrBoxObject = descr.boxObject;
  var descrRect = descr.getBoundingClientRect();
  var deltaX = descrBoxObject.screenX - descrRect.left;
  var deltaY = descrBoxObject.screenY - descrRect.top;
  aNode.parentNode.removeChild(descr);

  var rect = aNode.getBoundingClientRect();
  return [rect.left + deltaX, rect.top + deltaY];
}
