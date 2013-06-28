const Cu = Components.utils;
const PREF_UTTERANCE_ORDER = "accessibility.accessfu.utterance";

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import("resource://gre/modules/accessibility/OutputGenerator.jsm", this);

/**
 * Test context output generation.
 *
 * @param expected {Array} expected output.
 * @param aAccOrElmOrID    identifier to get an accessible to test.
 * @param aOldAccOrElmOrID optional identifier to get an accessible relative to
 *                         the |aAccOrElmOrID|.
 * @param aGenerator       the output generator to use when generating accessible
 *                         output
 *
 * Note: if |aOldAccOrElmOrID| is not provided, the |aAccOrElmOrID| must be
 * scoped to the "root" element in markup.
 */
function testContextOutput(expected, aAccOrElmOrID, aOldAccOrElmOrID, aGenerator) {
  aOldAccOrElmOrID = aOldAccOrElmOrID || "root";
  var accessible = getAccessible(aAccOrElmOrID);
  var oldAccessible = getAccessible(aOldAccOrElmOrID);
  var context = new PivotContext(accessible, oldAccessible);
  var output = aGenerator.genForContext(context);
  isDeeply(output, expected,
    "Context output is correct for " + aAccOrElmOrID);
}

/**
 * Test object output generated array that includes names.
 * Note: test ignores outputs without the name.
 *
 * @param aAccOrElmOrID identifier to get an accessible to test.
 * @param aGenerator    the output generator to use when generating accessible
 *                      output
 */
function testObjectOutput(aAccOrElmOrID, aGenerator) {
  var accessible = getAccessible(aAccOrElmOrID);
  var context = new PivotContext(accessible);
  var output = aGenerator.genForObject(accessible, context);
  var outputOrder;
  try {
    outputOrder = SpecialPowers.getIntPref(PREF_UTTERANCE_ORDER);
  } catch (ex) {
    // PREF_UTTERANCE_ORDER not set.
    outputOrder = 0;
  }
  var expectedNameIndex = outputOrder === 0 ? output.length - 1 : 0;
  var nameIndex = output.indexOf(accessible.name);

  if (nameIndex > -1) {
    ok(output.indexOf(accessible.name) === expectedNameIndex,
      "Object output is correct for " + aAccOrElmOrID);
  }
}

/**
 * Test object and context output for an accessible.
 *
 * @param expected {Array} expected output.
 * @param aAccOrElmOrID    identifier to get an accessible to test.
 * @param aOldAccOrElmOrID optional identifier to get an accessible relative to
 *                         the |aAccOrElmOrID|.
 * @param aOutputKind      the type of output
 */
function testOutput(expected, aAccOrElmOrID, aOldAccOrElmOrID, aOutputKind) {
  var generator;
  if (aOutputKind === 1) {
    generator = UtteranceGenerator;
  } else {
    generator = BrailleGenerator;
  }
  testContextOutput(expected, aAccOrElmOrID, aOldAccOrElmOrID, generator);
  // Just need to test object output for individual
  // accOrElmOrID.
  if (aOldAccOrElmOrID) {
    return;
  }
  testObjectOutput(aAccOrElmOrID, generator);
}
