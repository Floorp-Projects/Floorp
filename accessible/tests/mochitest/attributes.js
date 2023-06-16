/* import-globals-from common.js */

// //////////////////////////////////////////////////////////////////////////////
// Object attributes.

/**
 * Test object attributes.
 *
 * @param aAccOrElmOrID         [in] the accessible identifier
 * @param aAttrs                [in] the map of expected object attributes
 *                              (name/value pairs)
 * @param aSkipUnexpectedAttrs  [in] points this function doesn't fail if
 *                              unexpected attribute is encountered
 * @param aTodo                 [in] true if this is a 'todo'
 */
function testAttrs(aAccOrElmOrID, aAttrs, aSkipUnexpectedAttrs, aTodo) {
  testAttrsInternal(aAccOrElmOrID, aAttrs, aSkipUnexpectedAttrs, null, aTodo);
}

/**
 * Test object attributes that must not be present.
 *
 * @param aAccOrElmOrID         [in] the accessible identifier
 * @param aAbsentAttrs          [in] map of attributes that should not be
 *                              present (name/value pairs)
 * @param aTodo                 [in] true if this is a 'todo'
 */
function testAbsentAttrs(aAccOrElmOrID, aAbsentAttrs, aTodo) {
  testAttrsInternal(aAccOrElmOrID, {}, true, aAbsentAttrs, aTodo);
}

/**
 * Test object attributes that aren't right, but should be (todo)
 *
 * @param aAccOrElmOrID         [in] the accessible identifier
 * @param aKey                  [in] attribute name
 * @param aExpectedValue        [in] expected attribute value
 */
function todoAttr(aAccOrElmOrID, aKey, aExpectedValue) {
  testAttrs(
    aAccOrElmOrID,
    Object.fromEntries([[aKey, aExpectedValue]]),
    true,
    true
  );
}

/**
 * Test CSS based object attributes.
 */
function testCSSAttrs(aID) {
  var node = document.getElementById(aID);
  var computedStyle = document.defaultView.getComputedStyle(node);

  var attrs = {
    display: computedStyle.display,
    "text-align": computedStyle.textAlign,
    "text-indent": computedStyle.textIndent,
    "margin-left": computedStyle.marginLeft,
    "margin-right": computedStyle.marginRight,
    "margin-top": computedStyle.marginTop,
    "margin-bottom": computedStyle.marginBottom,
  };
  testAttrs(aID, attrs, true);
}

/**
 * Test the accessible that it doesn't have CSS-based object attributes.
 */
function testAbsentCSSAttrs(aID) {
  var attrs = {
    display: "",
    "text-align": "",
    "text-indent": "",
    "margin-left": "",
    "margin-right": "",
    "margin-top": "",
    "margin-bottom": "",
  };
  testAbsentAttrs(aID, attrs);
}

/**
 * Test group object attributes (posinset, setsize and level) and
 * nsIAccessible::groupPosition() method.
 *
 * @param aAccOrElmOrID  [in] the ID, DOM node or accessible
 * @param aPosInSet      [in] the value of 'posinset' attribute
 * @param aSetSize       [in] the value of 'setsize' attribute
 * @param aLevel         [in, optional] the value of 'level' attribute
 */
function testGroupAttrs(aAccOrElmOrID, aPosInSet, aSetSize, aLevel, aTodo) {
  var acc = getAccessible(aAccOrElmOrID);
  var levelObj = {},
    posInSetObj = {},
    setSizeObj = {};
  acc.groupPosition(levelObj, setSizeObj, posInSetObj);

  let groupPos = {},
    expectedGroupPos = {};

  if (aPosInSet && aSetSize) {
    groupPos.setsize = String(setSizeObj.value);
    groupPos.posinset = String(posInSetObj.value);

    expectedGroupPos.setsize = String(aSetSize);
    expectedGroupPos.posinset = String(aPosInSet);
  }

  if (aLevel) {
    groupPos.level = String(levelObj.value);

    expectedGroupPos.level = String(aLevel);
  }

  compareSimpleObjects(
    groupPos,
    expectedGroupPos,
    false,
    "wrong groupPos",
    aTodo
  );

  testAttrs(aAccOrElmOrID, expectedGroupPos, true, aTodo);
}

function testGroupParentAttrs(
  aAccOrElmOrID,
  aChildItemCount,
  aIsHierarchical,
  aTodo
) {
  testAttrs(
    aAccOrElmOrID,
    { "child-item-count": String(aChildItemCount) },
    true,
    aTodo
  );

  if (aIsHierarchical) {
    testAttrs(aAccOrElmOrID, { tree: "true" }, true, aTodo);
  } else {
    testAbsentAttrs(aAccOrElmOrID, { tree: "true" });
  }
}

// //////////////////////////////////////////////////////////////////////////////
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
function testTextAttrs(
  aID,
  aOffset,
  aAttrs,
  aDefAttrs,
  aStartOffset,
  aEndOffset,
  aSkipUnexpectedAttrs
) {
  var accessible = getAccessible(aID, [nsIAccessibleText]);
  if (!accessible) {
    return;
  }

  var startOffset = { value: -1 };
  var endOffset = { value: -1 };

  // do not include attributes exposed on hyper text accessible
  var attrs = getTextAttributes(
    aID,
    accessible,
    false,
    aOffset,
    startOffset,
    endOffset
  );

  if (!attrs) {
    return;
  }

  var errorMsg = " for " + aID + " at offset " + aOffset;

  is(startOffset.value, aStartOffset, "Wrong start offset" + errorMsg);
  is(endOffset.value, aEndOffset, "Wrong end offset" + errorMsg);

  compareAttrs(errorMsg, attrs, aAttrs, aSkipUnexpectedAttrs);

  // include attributes exposed on hyper text accessible
  var expectedAttrs = {};
  for (let name in aAttrs) {
    expectedAttrs[name] = aAttrs[name];
  }

  for (let name in aDefAttrs) {
    if (!(name in expectedAttrs)) {
      expectedAttrs[name] = aDefAttrs[name];
    }
  }

  attrs = getTextAttributes(
    aID,
    accessible,
    true,
    aOffset,
    startOffset,
    endOffset
  );

  if (!attrs) {
    return;
  }

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
function testDefaultTextAttrs(aID, aDefAttrs, aSkipUnexpectedAttrs) {
  var accessible = getAccessible(aID, [nsIAccessibleText]);
  if (!accessible) {
    return;
  }

  var defAttrs = null;
  try {
    defAttrs = accessible.defaultTextAttributes;
  } catch (e) {}

  if (!defAttrs) {
    ok(false, "Can't get default text attributes for " + aID);
    return;
  }

  var errorMsg = ". Getting default text attributes for " + aID;
  compareAttrs(errorMsg, defAttrs, aDefAttrs, aSkipUnexpectedAttrs);
}

/**
 * Test text attributes for wrong offset.
 */
function testTextAttrsWrongOffset(aID, aOffset) {
  var res = false;
  try {
    var s = {},
      e = {};
    // Bug 1602031
    // eslint-disable-next-line no-undef
    var acc = getAccessible(ID, [nsIAccessibleText]);
    acc.getTextAttributes(false, 157, s, e);
  } catch (ex) {
    res = true;
  }

  ok(
    res,
    "text attributes are calculated successfully at wrong offset " +
      aOffset +
      " for " +
      prettyName(aID)
  );
}

const kNormalFontWeight = function equalsToNormal(aWeight) {
  return aWeight <= 400;
};

const kBoldFontWeight = function equalsToBold(aWeight) {
  return aWeight > 400;
};

let isNNT = SpecialPowers.getBoolPref("widget.non-native-theme.enabled");
// The pt font size of the input element can vary by Linux distro.
const kInputFontSize =
  WIN || (MAC && isNNT)
    ? "10pt"
    : MAC
    ? "8pt"
    : function () {
        return true;
      };

const kAbsentFontFamily = function (aFontFamily) {
  return aFontFamily != "sans-serif";
};
const kInputFontFamily = function (aFontFamily) {
  return aFontFamily != "sans-serif";
};

const kMonospaceFontFamily = function (aFontFamily) {
  return aFontFamily != "monospace";
};
const kSansSerifFontFamily = function (aFontFamily) {
  return aFontFamily != "sans-serif";
};
const kSerifFontFamily = function (aFontFamily) {
  return aFontFamily != "serif";
};

const kCursiveFontFamily = LINUX ? "DejaVu Serif" : "Comic Sans MS";

/**
 * Return used font from the given computed style.
 */
function fontFamily(aComputedStyle) {
  var name = aComputedStyle.fontFamily;
  switch (name) {
    case "monospace":
      return kMonospaceFontFamily;
    case "sans-serif":
      return kSansSerifFontFamily;
    case "serif":
      return kSerifFontFamily;
    default:
      return name;
  }
}

/**
 * Returns a computed system color for this document.
 */
function getSystemColor(aColor) {
  let { r, g, b, a } = InspectorUtils.colorToRGBA(aColor, document);
  return a == 1 ? `rgb(${r}, ${g}, ${b})` : `rgba(${r}, ${g}, ${b}, ${a})`;
}

/**
 * Build an object of default text attributes expected for the given accessible.
 *
 * @param aID          [in] identifier of accessible
 * @param aFontSize    [in] font size
 * @param aFontWeight  [in, optional] kBoldFontWeight or kNormalFontWeight,
 *                      default value is kNormalFontWeight
 */
function buildDefaultTextAttrs(aID, aFontSize, aFontWeight, aFontFamily) {
  var elm = getNode(aID);
  var computedStyle = document.defaultView.getComputedStyle(elm);
  var bgColor =
    computedStyle.backgroundColor == "rgba(0, 0, 0, 0)"
      ? getSystemColor("Canvas")
      : computedStyle.backgroundColor;

  var defAttrs = {
    "font-style": computedStyle.fontStyle,
    "font-size": aFontSize,
    "background-color": bgColor,
    "font-weight": aFontWeight ? aFontWeight : kNormalFontWeight,
    color: computedStyle.color,
    "font-family": aFontFamily ? aFontFamily : fontFamily(computedStyle),
    "text-position": computedStyle.verticalAlign,
  };

  return defAttrs;
}

// //////////////////////////////////////////////////////////////////////////////
// Private.

function getTextAttributes(
  aID,
  aAccessible,
  aIncludeDefAttrs,
  aOffset,
  aStartOffset,
  aEndOffset
) {
  // This function expects the passed in accessible to already be queried for
  // nsIAccessibleText.
  var attrs = null;
  try {
    attrs = aAccessible.getTextAttributes(
      aIncludeDefAttrs,
      aOffset,
      aStartOffset,
      aEndOffset
    );
  } catch (e) {}

  if (attrs) {
    return attrs;
  }

  ok(false, "Can't get text attributes for " + aID);
  return null;
}

function testAttrsInternal(
  aAccOrElmOrID,
  aAttrs,
  aSkipUnexpectedAttrs,
  aAbsentAttrs,
  aTodo
) {
  var accessible = getAccessible(aAccOrElmOrID);
  if (!accessible) {
    return;
  }

  var attrs = null;
  try {
    attrs = accessible.attributes;
  } catch (e) {}

  if (!attrs) {
    ok(false, "Can't get object attributes for " + prettyName(aAccOrElmOrID));
    return;
  }

  var errorMsg = " for " + prettyName(aAccOrElmOrID);
  compareAttrs(
    errorMsg,
    attrs,
    aAttrs,
    aSkipUnexpectedAttrs,
    aAbsentAttrs,
    aTodo
  );
}

function compareAttrs(
  aErrorMsg,
  aAttrs,
  aExpectedAttrs,
  aSkipUnexpectedAttrs,
  aAbsentAttrs,
  aTodo
) {
  // Check if all obtained attributes are expected and have expected value.
  let attrObject = {};
  for (let prop of aAttrs.enumerate()) {
    attrObject[prop.key] = prop.value;
  }

  // Create expected attributes set by using the return values from
  // embedded functions to determine the entry's value.
  let expectedObj = Object.fromEntries(
    Object.entries(aExpectedAttrs).map(([k, v]) => {
      if (v instanceof Function) {
        // If value is a function that returns true given the received
        // attribute value, assign the attribute value to the entry.
        // If it is false, stringify the function for good error reporting.
        let value = v(attrObject[k]) ? attrObject[k] : v.toString();
        return [k, value];
      }

      return [k, v];
    })
  );

  compareSimpleObjects(
    attrObject,
    expectedObj,
    aSkipUnexpectedAttrs,
    aErrorMsg,
    aTodo
  );

  // Check if all unexpected attributes are absent.
  if (aAbsentAttrs) {
    let presentAttrs = Object.keys(attrObject).filter(
      k => aAbsentAttrs[k] !== undefined
    );
    if (presentAttrs.length) {
      (aTodo ? todo : ok)(
        false,
        `There were unexpected attributes: ${presentAttrs}`
      );
    }
  }
}

function compareSimpleObjects(
  aObj,
  aExpectedObj,
  aSkipUnexpectedAttrs,
  aMessage,
  aTodo
) {
  let keys = aSkipUnexpectedAttrs
    ? Object.keys(aExpectedObj).sort()
    : Object.keys(aObj).sort();
  let o1 = JSON.stringify(aObj, keys);
  let o2 = JSON.stringify(aExpectedObj, keys);

  if (aTodo) {
    todo_is(o1, o2, `${aMessage} - Got ${o1}, expected ${o2}`);
  } else {
    is(o1, o2, aMessage);
  }
}
