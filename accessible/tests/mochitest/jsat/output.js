var Cu = Components.utils;
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
  var accessible = getAccessible(aAccOrElmOrID);
  var oldAccessible = aOldAccOrElmOrID !== null ?
    getAccessible(aOldAccOrElmOrID || 'root') : null;
  var context = new PivotContext(accessible, oldAccessible);
  var output = aGenerator.genForContext(context);

  // Create a version of the output that has null members where we have
  // null members in the expected output. Those are indexes that are not testable
  // because of the changing nature of the test (different window names), or strings
  // that are inaccessible to us, like the title of parent documents.
  var masked_output = [];
  for (var i=0; i < output.length; i++) {
    if (expected[i] === null) {
      masked_output.push(null);
    } else {
      masked_output[i] = typeof output[i] === "string" ? output[i].trim() :
        output[i];
    }
  }

  isDeeply(masked_output, expected,
           "Context output is correct for " + aAccOrElmOrID +
           " (output: " + JSON.stringify(output) + ") ==" +
           " (expected: " + JSON.stringify(expected) + ")");
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
  if (!accessible.name || !accessible.name.trim()) {
    return;
  }
  var context = new PivotContext(accessible);
  var output = aGenerator.genForObject(accessible, context);
  var outputOrder;
  // eslint-disable-next-line mozilla/use-default-preference-values
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

function testHints(expected, aAccOrElmOrID, aOldAccOrElmOrID) {
  var accessible = getAccessible(aAccOrElmOrID);
  var oldAccessible = aOldAccOrElmOrID !== null ?
  getAccessible(aOldAccOrElmOrID || 'root') : null;
  var context = new PivotContext(accessible, oldAccessible);
  var hints = context.interactionHints;

  isDeeply(hints, expected,
           "Context hitns are correct for " + aAccOrElmOrID +
           " (hints: " + JSON.stringify(hints) + ") ==" +
           " (expected: " + JSON.stringify(expected) + ")");
}
