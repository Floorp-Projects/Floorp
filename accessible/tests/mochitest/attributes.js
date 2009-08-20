////////////////////////////////////////////////////////////////////////////////
// Object attributes.

/**
 * Test object attributes.
 *
 * @param aAccOrElmOrID         [in] the accessible identifier
 * @param aAttrs                [in] the map of expected object attributes
 *                              (name/value pairs)
 * @param aSkipUnexpectedAttrs  [in] points this function doesn't fail if
 *                              unexpected attribute is encountered
 */
function testAttrs(aAccOrElmOrID, aAttrs, aSkipUnexpectedAttrs)
{
  testAttrsInternal(aAccOrElmOrID, aAttrs, aSkipUnexpectedAttrs);
}

/**
 * Test object attributes that must not be present.
 *
 * @param aAccOrElmOrID         [in] the accessible identifier
 * @param aAbsentAttrs          [in] map of attributes that should not be
 *                              present (name/value pairs)
 */
function testAbsentAttrs(aAccOrElmOrID, aAbsentAttrs, aSkipUnexpectedAttrs)
{
  testAttrsInternal(aAccOrElmOrID, {}, true, aAbsentAttrs);
}

/**
 * Test group object attributes (posinset, setsize and level)
 *
 * @param aAccOrElmOrID  [in] the ID, DOM node or accessible
 * @param aPosInSet      [in] the value of 'posinset' attribute
 * @param aSetSize       [in] the value of 'setsize' attribute
 * @param aLevel         [in, optional] the value of 'level' attribute
 */
function testGroupAttrs(aAccOrElmOrID, aPosInSet, aSetSize, aLevel)
{
  var attrs = {
    "posinset": String(aPosInSet),
    "setsize": String(aSetSize)
  };

  if (aLevel)
    attrs["level"] = String(aLevel);

  testAttrs(aAccOrElmOrID, attrs, true);
}

////////////////////////////////////////////////////////////////////////////////
// Text attributes.

/**
 * Test text attributes.
 *
 * @param aID                   [in] the ID of DOM element having text
 *                              accessible
 * @param aOffset               [in] the offset inside text accessible to fetch
 *                              text attributes
 * @param aAttrs                [in] the map of expected text attributes
 *                              (name/value pairs) exposed at the offset
 * @param aDefAttrs             [in] the map of expected text attributes
 *                              (name/value pairs) exposed on hyper text
 *                              accessible
 * @param aStartOffset          [in] expected start offset where text attributes
 *                              are applied
 * @param aEndOffset            [in] expected end offset where text attribute
 *                              are applied
 * @param aSkipUnexpectedAttrs  [in] points the function doesn't fail if
 *                              unexpected attribute is encountered
 */
function testTextAttrs(aID, aOffset, aAttrs, aDefAttrs,
                       aStartOffset, aEndOffset, aSkipUnexpectedAttrs)
{
  var accessible = getAccessible(aID, [nsIAccessibleText]);
  if (!accessible)
    return;

  var startOffset = { value: -1 };
  var endOffset = { value: -1 };

  // do not include attributes exposed on hyper text accessbile
  var attrs = getTextAttributes(aID, accessible, false, aOffset,
                                startOffset, endOffset);

  if (!attrs)
    return;

  var errorMsg = " for " + aID + " at offset " + aOffset;

  is(startOffset.value, aStartOffset, "Wrong start offset" + errorMsg);
  is(endOffset.value, aEndOffset, "Wrong end offset" + errorMsg);

  compareAttrs(errorMsg, attrs, aAttrs, aSkipUnexpectedAttrs);

  // include attributes exposed on hyper text accessbile
  var expectedAttrs = {};
  for (var name in aAttrs)
    expectedAttrs[name] = aAttrs[name];

  for (var name in aDefAttrs) {
    if (!(name in expectedAttrs))
      expectedAttrs[name] = aDefAttrs[name];
  }

  attrs = getTextAttributes(aID, accessible, true, aOffset,
                            startOffset, endOffset);
  
  if (!attrs)
    return;

  compareAttrs(errorMsg, attrs, expectedAttrs, aSkipUnexpectedAttrs);
}

/**
 * Test default text attributes.
 *
 * @param aID                   [in] the ID of DOM element having text
 *                              accessible
 * @param aDefAttrs             [in] the map of expected text attributes
 *                              (name/value pairs)
 * @param aSkipUnexpectedAttrs  [in] points the function doesn't fail if
 *                              unexpected attribute is encountered
 */
function testDefaultTextAttrs(aID, aDefAttrs, aSkipUnexpectedAttrs)
{
  var accessible = getAccessible(aID, [nsIAccessibleText]);
  if (!accessible)
    return;
  
  var defAttrs = null;
  try{
    defAttrs = accessible.defaultTextAttributes;
  } catch (e) {
  }
  
  if (!defAttrs) {
    ok(false, "Can't get default text attributes for " + aID);
    return;
  }
  
  var errorMsg = ". Getting default text attributes for " + aID;
  compareAttrs(errorMsg, defAttrs, aDefAttrs, aSkipUnexpectedAttrs);
}

////////////////////////////////////////////////////////////////////////////////
// Private.

function getTextAttributes(aID, aAccessible, aIncludeDefAttrs, aOffset,
                           aStartOffset, aEndOffset)
{
  // This function expects the passed in accessible to already be queried for
  // nsIAccessibleText.
  var attrs = null;
  try {
    attrs = aAccessible.getTextAttributes(aIncludeDefAttrs, aOffset,
                                          aStartOffset, aEndOffset);
  } catch (e) {
  }

  if (attrs)
    return attrs;

  ok(false, "Can't get text attributes for " + aID);
  return null;
}

function testAttrsInternal(aAccOrElmOrID, aAttrs, aSkipUnexpectedAttrs,
                   aAbsentAttrs)
{
  var accessible = getAccessible(aAccOrElmOrID);
  if (!accessible)
    return;

  var attrs = null;
  try {
    attrs = accessible.attributes;
  } catch (e) { }
  
  if (!attrs) {
    ok(false, "Can't get object attributes for " + prettyName(aAccOrElmOrID));
    return;
  }
  
  var errorMsg = " for " + prettyName(aAccOrElmOrID);
  compareAttrs(errorMsg, attrs, aAttrs, aSkipUnexpectedAttrs, aAbsentAttrs);
}

function compareAttrs(aErrorMsg, aAttrs, aExpectedAttrs, aSkipUnexpectedAttrs,
                      aAbsentAttrs)
{
  var enumerate = aAttrs.enumerate();
  while (enumerate.hasMoreElements()) {
    var prop = enumerate.getNext().QueryInterface(nsIPropertyElement);

    if (!(prop.key in aExpectedAttrs)) {
      if (!aSkipUnexpectedAttrs)
        ok(false, "Unexpected attribute '" + prop.key + "' having '" +
           prop.value + "'" + aErrorMsg);
    } else {
      var msg = "Attribute '" + prop.key + "' has wrong value" + aErrorMsg;
      var expectedValue = aExpectedAttrs[prop.key];

      if (typeof expectedValue == "function")
        ok(expectedValue(prop.value), msg);
      else
        is(prop.value, expectedValue, msg);
    }
  }

  for (var name in aExpectedAttrs) {
    var value = "";
    try {
      value = aAttrs.getStringProperty(name);
    } catch(e) { }

    if (!value)
      ok(false,
         "There is no expected attribute '" + name + "' " + aErrorMsg);
  }

  if (aAbsentAttrs)
    for (var name in aAbsentAttrs) {
      var value = "";
      try {
        value = aAttrs.getStringProperty(name);
      } catch(e) { }

      if (value)
        ok(false,
           "There is an unexpected attribute '" + name + "' " + aErrorMsg);
      else
        ok(true,
           "There is no unexpected attribute '" + name + "' " + aErrorMsg);
    }
}
