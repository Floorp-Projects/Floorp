const Cu = Components.utils;
const PREF_UTTERANCE_ORDER = "accessibility.accessfu.utterance";

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import("resource://gre/modules/accessibility/UtteranceGenerator.jsm",
  this);

/**
 * Test context utterance generation.
 *
 * @param expected {Array} expected utterance.
 * @param aAccOrElmOrID    identifier to get an accessible to test.
 * @param aOldAccOrElmOrID optional identifier to get an accessible relative to
 *                         the |aAccOrElmOrID|.
 *
 * Note: if |aOldAccOrElmOrID| is not provided, the |aAccOrElmOrID| must be
 * scoped to the "root" element in markup.
 */
function testContextUtterance(expected, aAccOrElmOrID, aOldAccOrElmOrID) {
  aOldAccOrElmOrID = aOldAccOrElmOrID || "root";
  var accessible = getAccessible(aAccOrElmOrID);
  var oldAccessible = getAccessible(aOldAccOrElmOrID);
  var context = new PivotContext(accessible, oldAccessible);
  var utterance = UtteranceGenerator.genForContext(context);
  isDeeply(utterance, expected,
    "Context utterance is correct for " + aAccOrElmOrID);
}

/**
 * Test object utterance generated array that includes names.
 * Note: test ignores utterances without the name.
 *
 * @param aAccOrElmOrID identifier to get an accessible to test.
 */
function testObjectUtterance(aAccOrElmOrID) {
  var accessible = getAccessible(aAccOrElmOrID);
  var utterance = UtteranceGenerator.genForObject(accessible);
  var utteranceOrder;
  try {
    utteranceOrder = SpecialPowers.getIntPref(PREF_UTTERANCE_ORDER);
  } catch (ex) {
    // PREF_UTTERANCE_ORDER not set.
    utteranceOrder = 0;
  }
  var expectedNameIndex = utteranceOrder === 0 ? utterance.length - 1 : 0;
  var nameIndex = utterance.indexOf(accessible.name);

  if (nameIndex > -1) {
    ok(utterance.indexOf(accessible.name) === expectedNameIndex,
      "Object utterance is correct for " + aAccOrElmOrID);
  }
}

/**
 * Test object and context utterance for an accessible.
 *
 * @param expected {Array} expected utterance.
 * @param aAccOrElmOrID    identifier to get an accessible to test.
 * @param aOldAccOrElmOrID optional identifier to get an accessible relative to
 *                         the |aAccOrElmOrID|.
 */
function testUtterance(expected, aAccOrElmOrID, aOldAccOrElmOrID) {
  testContextUtterance(expected, aAccOrElmOrID, aOldAccOrElmOrID);
  // Just need to test object utterance for individual
  // accOrElmOrID.
  if (aOldAccOrElmOrID) {
    return;
  }
  testObjectUtterance(aAccOrElmOrID);
}