/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * reflect.js is a collection of methods to test HTML attribute reflection.
 * Each of attribute is reflected differently, depending on various parameters,
 * see:
 * http://www.whatwg.org/html/#reflecting-content-attributes-in-idl-attributes
 *
 * Do not forget to add these line at the beginning of each new reflect* method:
 * ok(attr in element, attr + " should be an IDL attribute of this element");
 * is(typeof element[attr], <type>, attr + " IDL attribute should be a <type>");
 */

/**
 * Checks that a given attribute is correctly reflected as a string.
 *
 * @param aParameters   Object    object containing the parameters, which are:
 *  - element           Element   node to test
 *  - attribute         String    name of the attribute
 *     OR
 *    attribute         Object    object containing two attributes, 'content' and 'idl'
 *  - otherValues       Array     [optional] other values to test in addition of the default ones
 *  - extendedAttributes Object   object which can have 'TreatNullAs': "EmptyString"
 */
function reflectString(aParameters)
{
  var element = aParameters.element;
  var contentAttr = typeof aParameters.attribute === "string"
                      ? aParameters.attribute : aParameters.attribute.content;
  var idlAttr = typeof aParameters.attribute === "string"
                  ? aParameters.attribute : aParameters.attribute.idl;
  var otherValues = aParameters.otherValues !== undefined
                      ? aParameters.otherValues : [];
  var treatNullAs = aParameters.extendedAttributes ?
        aParameters.extendedAttributes.TreatNullAs : null;

  ok(idlAttr in element,
     idlAttr + " should be an IDL attribute of this element");
  is(typeof element[idlAttr], "string",
     "'" + idlAttr + "' IDL attribute should be a string");

  // Tests when the attribute isn't set.
  is(element.getAttribute(contentAttr), null,
     "When not set, the content attribute should be null.");
  is(element[idlAttr], "",
     "When not set, the IDL attribute should return the empty string");

  /**
   * TODO: as long as null stringification doesn't follow the WebIDL
   * specifications, don't add it to the loop below and keep it here.
   */
  element.setAttribute(contentAttr, null);
  is(element.getAttribute(contentAttr), "null",
     "null should have been stringified to 'null' for '" + contentAttr + "'");
  is(element[idlAttr], "null",
      "null should have been stringified to 'null' for '" + idlAttr + "'");
  element.removeAttribute(contentAttr);

  element[idlAttr] = null;
  if (treatNullAs == "EmptyString") {
    is(element.getAttribute(contentAttr), "",
       "null should have been stringified to '' for '" + contentAttr + "'");
    is(element[idlAttr], "",
       "null should have been stringified to '' for '" + idlAttr + "'");
  } else {
    is(element.getAttribute(contentAttr), "null",
       "null should have been stringified to 'null' for '" + contentAttr + "'");
    is(element[idlAttr], "null",
       "null should have been stringified to 'null' for '" + contentAttr + "'");
  }
  element.removeAttribute(contentAttr);

  // Tests various strings.
  var stringsToTest = [
    // [ test value, expected result ]
    [ "", "" ],
    [ "null", "null" ],
    [ "undefined", "undefined" ],
    [ "foo", "foo" ],
    [ contentAttr, contentAttr ],
    [ idlAttr, idlAttr ],
    // TODO: uncomment this when null stringification will follow the specs.
    // [ null, "null" ],
    [ undefined, "undefined" ],
    [ true, "true" ],
    [ false, "false" ],
    [ 42, "42" ],
    // ES5, verse 8.12.8.
    [ { toString: function() { return "foo" } },
      "foo" ],
    [ { valueOf: function() { return "foo" } },
      "[object Object]" ],
    [ { valueOf: function() { return "quux" },
       toString: undefined },
      "quux" ],
    [ { valueOf: function() { return "foo" },
        toString: function() { return "bar" } },
      "bar" ]
  ];

  otherValues.forEach(function(v) { stringsToTest.push([v, v]) });

  stringsToTest.forEach(function([v, r]) {
    element.setAttribute(contentAttr, v);
    is(element[idlAttr], r,
       "IDL attribute '" + idlAttr + "' should return the value it has been set to.");
    is(element.getAttribute(contentAttr), r,
       "Content attribute '" + contentAttr + "'should return the value it has been set to.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v;
    is(element[idlAttr], r,
       "IDL attribute '" + idlAttr + "' should return the value it has been set to.");
    is(element.getAttribute(contentAttr), r,
       "Content attribute '" + contentAttr + "' should return the value it has been set to.");
    element.removeAttribute(contentAttr);
  });

  // Tests after removeAttribute() is called. Should be equivalent with not set.
  is(element.getAttribute(contentAttr), null,
     "When not set, the content attribute should be null.");
  is(element[idlAttr], "",
     "When not set, the IDL attribute should return the empty string");
}

/**
 * Checks that a given attribute name for a given element is correctly reflected
 * as an unsigned int.
 *
 * @param aParameters   Object    object containing the parameters, which are:
 *  - element           Element   node to test on
 *  - attribute         String    name of the attribute
 *  - nonZero           Boolean   whether the attribute should be non-null
 *  - defaultValue      Integer   [optional] default value, if different from the default one
 */
function reflectUnsignedInt(aParameters)
{
  var element = aParameters.element;
  var attr = aParameters.attribute;
  var nonZero = aParameters.nonZero;
  var defaultValue = aParameters.defaultValue;

  if (defaultValue === undefined) {
    if (nonZero) {
      defaultValue = 1;
    } else {
      defaultValue = 0;
    }
  }

  ok(attr in element, attr + " should be an IDL attribute of this element");
  is(typeof element[attr], "number", attr + " IDL attribute should be a number");

  // Check default value.
  is(element[attr], defaultValue, "default value should be " + defaultValue);
  ok(!element.hasAttribute(attr), attr + " shouldn't be present");

  var values = [ 1, 3, 42, 2147483647 ];

  for (var value of values) {
    element[attr] = value;
    is(element[attr], value, "." + attr + " should be equals " + value);
    is(element.getAttribute(attr), value,
       "@" + attr + " should be equals " + value);

    element.setAttribute(attr, value);
    is(element[attr], value, "." + attr + " should be equals " + value);
    is(element.getAttribute(attr), value,
       "@" + attr + " should be equals " + value);
  }

  // -3000000000 is equivalent to 1294967296 when using the IDL attribute.
  element[attr] = -3000000000;
  is(element[attr], 1294967296, "." + attr + " should be equals to 1294967296");
  is(element.getAttribute(attr), 1294967296,
     "@" + attr + " should be equals to 1294967296");

  // When setting the content atribute, it's a string so it will be unvalid.
  element.setAttribute(attr, -3000000000);
  is(element.getAttribute(attr), -3000000000,
     "@" + attr + " should be equals to " + -3000000000);
  is(element[attr], defaultValue,
     "." + attr + " should be equals to " + defaultValue);

  var nonValidValues = [
    /* invalid value, value in the unsigned int range */
    [ -2147483648, 2147483648 ],
    [ -1,          4294967295 ],
    [ 3147483647,  3147483647 ],
  ];

  for (var values of nonValidValues) {
    element[attr] = values[0];
    is(element.getAttribute(attr), values[1],
       "@" + attr + " should be equals to " + values[1]);
    is(element[attr], defaultValue,
       "." + attr + " should be equals to " + defaultValue);
  }

  for (var values of nonValidValues) {
    element.setAttribute(attr, values[0]);
    is(element.getAttribute(attr), values[0],
       "@" + attr + " should be equals to " + values[0]);
    is(element[attr], defaultValue,
       "." + attr + " should be equals to " + defaultValue);
  }

  // Setting to 0 should throw an error if nonZero is true.
  var caught = false;
  try {
    element[attr] = 0;
  } catch(e) {
    caught = true;
    is(e.name, "IndexSizeError", "exception should be IndexSizeError");
    is(e.code, DOMException.INDEX_SIZE_ERR, "exception code should be INDEX_SIZE_ERR");
  }

  if (nonZero) {
    ok(caught, "an exception should have been caught");
  } else {
    ok(!caught, "no exception should have been caught");
  }

  // If 0 is set in @attr, it will be ignored when calling .attr.
  element.setAttribute(attr, 0);
  is(element.getAttribute(attr), 0, "@" + attr + " should be equals to 0");
  if (nonZero) {
    is(element[attr], defaultValue,
       "." + attr + " should be equals to " + defaultValue);
  } else {
    is(element[attr], 0, "." + attr + " should be equals to 0");
  }
}

/**
 * Checks that a given attribute is correctly reflected as limited to known
 * values enumerated attribute.
 *
 * @param aParameters     Object   object containing the parameters, which are:
 *  - element             Element  node to test on
 *  - attribute           String   name of the attribute
 *     OR
 *    attribute           Object   object containing two attributes, 'content' and 'idl'
 *  - validValues         Array    valid values we support
 *  - invalidValues       Array    invalid values
 *  - defaultValue        String   [optional] default value when no valid value is set
 *     OR
 *    defaultValue        Object   [optional] object containing two attributes, 'invalid' and 'missing'
 *  - unsupportedValues   Array    [optional] valid values we do not support
 */
function reflectLimitedEnumerated(aParameters)
{
  var element = aParameters.element;
  var contentAttr = typeof aParameters.attribute === "string"
                      ? aParameters.attribute : aParameters.attribute.content;
  var idlAttr = typeof aParameters.attribute === "string"
                  ? aParameters.attribute : aParameters.attribute.idl;
  var validValues = aParameters.validValues;
  var invalidValues = aParameters.invalidValues;
  var defaultValueInvalid = aParameters.defaultValue === undefined
                               ? "" : typeof aParameters.defaultValue === "string"
                                   ? aParameters.defaultValue : aParameters.defaultValue.invalid
  var defaultValueMissing = aParameters.defaultValue === undefined
                                ? "" : typeof aParameters.defaultValue === "string"
                                    ? aParameters.defaultValue : aParameters.defaultValue.missing
  var unsupportedValues = aParameters.unsupportedValues !== undefined
                            ? aParameters.unsupportedValues : [];

  ok(idlAttr in element, idlAttr + " should be an IDL attribute of this element");
  is(typeof element[idlAttr], "string", "'" + idlAttr + "' IDL attribute should be a string");

  // Explicitly check the default value.
  element.removeAttribute(contentAttr);
  is(element[idlAttr], defaultValueMissing,
     "When no attribute is set, the value should be the default value.");

  // Check valid values.
  validValues.forEach(function (v) {
    element.setAttribute(contentAttr, v);
    is(element[idlAttr], v,
       v + " should be accepted as a valid value for " + idlAttr);
    is(element.getAttribute(contentAttr), v,
       "Content attribute should return the value it has been set to.");
    element.removeAttribute(contentAttr);

    element.setAttribute(contentAttr, v.toUpperCase());
    is(element[idlAttr], v,
       "Enumerated attributes should be case-insensitive.");
    is(element.getAttribute(contentAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v;
    is(element[idlAttr], v,
       v + " should be accepted as a valid value for " + idlAttr);
    is(element.getAttribute(contentAttr), v,
       "Content attribute should return the value it has been set to.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v.toUpperCase();
    is(element[idlAttr], v,
       "Enumerated attributes should be case-insensitive.");
    is(element.getAttribute(contentAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    element.removeAttribute(contentAttr);
  });

  // Check invalid values.
  invalidValues.forEach(function (v) {
    element.setAttribute(contentAttr, v);
    is(element[idlAttr], defaultValueInvalid,
       "When the content attribute is set to an invalid value, the default value should be returned.");
    is(element.getAttribute(contentAttr), v,
       "Content attribute should not have been changed.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v;
    is(element[idlAttr], defaultValueInvalid,
       "When the value is set to an invalid value, the default value should be returned.");
    is(element.getAttribute(contentAttr), v,
       "Content attribute should not have been changed.");
    element.removeAttribute(contentAttr);
  });

  // Check valid values we currently do not support.
  // Basically, it's like the checks for the valid values but with some todo's.
  unsupportedValues.forEach(function (v) {
    element.setAttribute(contentAttr, v);
    todo_is(element[idlAttr], v,
            v + " should be accepted as a valid value for " + idlAttr);
    is(element.getAttribute(contentAttr), v,
       "Content attribute should return the value it has been set to.");
    element.removeAttribute(contentAttr);

    element.setAttribute(contentAttr, v.toUpperCase());
    todo_is(element[idlAttr], v,
            "Enumerated attributes should be case-insensitive.");
    is(element.getAttribute(contentAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v;
    todo_is(element[idlAttr], v,
            v + " should be accepted as a valid value for " + idlAttr);
    is(element.getAttribute(contentAttr), v,
       "Content attribute should return the value it has been set to.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v.toUpperCase();
    todo_is(element[idlAttr], v,
            "Enumerated attributes should be case-insensitive.");
    is(element.getAttribute(contentAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    element.removeAttribute(contentAttr);
  });
}

/**
 * Checks that a given attribute is correctly reflected as a boolean.
 *
 * @param aParameters    Object    object containing the parameters, which are:
 *  - element            Element   node to test on
 *  - attribute          String    name of the attribute
 *     OR
 *    attribute          Object    object containing two attributes, 'content' and 'idl'
 */
function reflectBoolean(aParameters)
{
  var element = aParameters.element;
  var contentAttr = typeof aParameters.attribute === "string"
                      ? aParameters.attribute : aParameters.attribute.content;
  var idlAttr = typeof aParameters.attribute === "string"
                  ? aParameters.attribute : aParameters.attribute.idl;

  ok(idlAttr in element,
     idlAttr + " should be an IDL attribute of this element");
  is(typeof element[idlAttr], "boolean",
     idlAttr + " IDL attribute should be a boolean");

  // Tests when the attribute isn't set.
  is(element.getAttribute(contentAttr), null,
     "When not set, the content attribute should be null.");
  is(element[idlAttr], false,
     "When not set, the IDL attribute should return false");

  /**
   * Test various values.
   * Each value to test is actually an object containing a 'value' property
   * containing the value to actually test, a 'stringified' property containing
   * the stringified value and a 'result' property containing the expected
   * result when the value is set to the IDL attribute.
   */
  var valuesToTest = [
    { value: true, stringified: "true", result: true },
    { value: false, stringified: "false", result: false },
    { value: "true", stringified: "true", result: true },
    { value: "false", stringified: "false", result: true },
    { value: "foo", stringified: "foo", result: true },
    { value: idlAttr, stringified: idlAttr, result: true },
    { value: contentAttr, stringified: contentAttr, result: true },
    { value: "null", stringified: "null", result: true },
    { value: "undefined", stringified: "undefined", result: true },
    { value: "", stringified: "", result: false },
    { value: undefined, stringified: "undefined", result: false },
    { value: null, stringified: "null", result: false },
    { value: +0, stringified: "0", result: false },
    { value: -0, stringified: "0", result: false },
    { value: NaN, stringified: "NaN", result: false },
    { value: 42, stringified: "42", result: true },
    { value: Infinity, stringified: "Infinity", result: true },
    { value: -Infinity, stringified: "-Infinity", result: true },
    // ES5, verse 9.2.
    { value: { toString: function() { return "foo" } }, stringified: "foo",
      result: true },
    { value: { valueOf: function() { return "foo" } },
      stringified: "[object Object]", result: true },
    { value: { valueOf: function() { return "quux" }, toString: undefined },
      stringified: "quux", result: true },
    { value: { valueOf: function() { return "foo" },
               toString: function() { return "bar" } }, stringified: "bar",
      result: true },
    { value: { valueOf: function() { return false } },
      stringified: "[object Object]", result: true },
    { value: { foo: false, bar: false }, stringified: "[object Object]",
      result: true },
    { value: { }, stringified: "[object Object]", result: true },
  ];

  valuesToTest.forEach(function(v) {
    element.setAttribute(contentAttr, v.value);
    is(element[idlAttr], true,
       "IDL attribute should return always return 'true' if the content attribute has been set");
    is(element.getAttribute(contentAttr), v.stringified,
       "Content attribute should return the stringified value it has been set to.");
    element.removeAttribute(contentAttr);

    element[idlAttr] = v.value;
    is(element[idlAttr], v.result, "IDL attribute should return " + v.result);
    is(element.getAttribute(contentAttr), v.result ? "" : null,
       v.result ? "Content attribute should return the empty string."
                : "Content attribute should return null.");
    is(element.hasAttribute(contentAttr), v.result,
       v.result ? contentAttr + " should not be present"
                : contentAttr + " should be present");
    element.removeAttribute(contentAttr);
  });

  // Tests after removeAttribute() is called. Should be equivalent with not set.
  is(element.getAttribute(contentAttr), null,
     "When not set, the content attribute should be null.");
  is(element[contentAttr], false,
     "When not set, the IDL attribute should return false");
}

/**
 * Checks that a given attribute name for a given element is correctly reflected
 * as an signed integer.
 *
 * @param aParameters   Object    object containing the parameters, which are:
 *  - element           Element   node to test on
 *  - attribute         String    name of the attribute
 *  - nonNegative       Boolean   true if the attribute is limited to 'non-negative numbers', false otherwise
 *  - defaultValue      Integer   [optional] default value, if one exists
 */
function reflectInt(aParameters)
{
  // Expected value returned by .getAttribute() when |value| has been previously passed to .setAttribute().
  function expectedGetAttributeResult(value) {
    return String(value);
  }

  function stringToInteger(value, nonNegative, defaultValue) {
    // Parse: Ignore leading whitespace, find [+/-][numbers]
    var result = /^[ \t\n\f\r]*([\+\-]?[0-9]+)/.exec(value);
    if (result) {
      if ((nonNegative ? 0:-0x80000000) <= result[1] && result[1] <= 0x7FFFFFFF) {
        // If the value is within allowed value range for signed/unsigned integer, return value
        return result[1];
      }
    }
    return defaultValue;
  }

  // Expected value returned by .getAttribute(attr) or .attr if |value| has been set via the IDL attribute.
  function expectedIdlAttributeResult(value) {
    // This returns the result of calling the ES ToInt32 algorithm on value.
    return value << 0;
  }

  var element = aParameters.element;
  var attr = aParameters.attribute;
  var nonNegative = aParameters.nonNegative;

  var defaultValue = aParameters.defaultValue !== undefined
                      ? aParameters.defaultValue
                      : nonNegative ? -1 : 0;

  ok(attr in element, attr + " should be an IDL attribute of this element");
  is(typeof element[attr], "number", attr + " IDL attribute should be a number");

  // Check default value.
  is(element[attr], defaultValue, "default value should be " + defaultValue);
  ok(!element.hasAttribute(attr), attr + " shouldn't be present");

  /**
   * Test various values.
   * value: The test value that will be set using both setAttribute(value) and
   *        element[attr] = value
   */
  var valuesToTest = [
    // Test numeric inputs up to max signed integer
    0, 1, 55555, 2147483647, +42,
    // Test string inputs up to max signed integer
    "0", "1", "777777", "2147483647", "+42",
    // Test negative numeric inputs up to min signed integer
    -0, -1, -3333, -2147483648,
    // Test negative string inputs up to min signed integer
    "-0", "-1", "-222", "-2147483647", "-2147483648",
    // Test numeric inputs that are outside legal 32 bit signed values
    -2147483649, -3000000000, -4294967296, 2147483649, 4000000000, -4294967297,
    // Test string inputs with extra padding
    "     1111111", "  23456   ",
    // Test non-numeric string inputs
    "", " ", "+", "-", "foo", "+foo", "-foo", "+     foo", "-     foo", "+-2", "-+2", "++2", "--2", "hello1234", "1234hello",
    "444 world 555", "why 567 what", "-3 nots", "2e5", "300e2", "42+-$", "+42foo", "-514not", "\vblah", "0x10FFFF", "-0xABCDEF",
    // Test decimal numbers
    1.2345, 42.0, 3456789.1, -2.3456, -6789.12345, -2147483649.1234,
    // Test decimal strings
    "1.2345", "42.0", "3456789.1", "-2.3456", "-6789.12345", "-2147483649.1234",
    // Test special values
    undefined, null, NaN, Infinity, -Infinity,
  ];

  valuesToTest.forEach(function(v) {
    var intValue = stringToInteger(v, nonNegative, defaultValue);

    element.setAttribute(attr, v);

    is(element.getAttribute(attr), expectedGetAttributeResult(v), element.localName + ".setAttribute(" +
      attr + ", " + v + "), " + element.localName + ".getAttribute(" + attr + ") ");

    if (intValue == -2147483648 && element[attr] == defaultValue) {
      //TBD: Bug 586761: .setAttribute(attr, -2147483648) --> element[attr] == defaultValue instead of -2147483648
      todo_is(element[attr], intValue, "Bug 586761: " + element.localName +
        ".setAttribute(value, " + v + "), " + element.localName + "[" + attr + "] ");
    } else {
      is(element[attr], intValue, element.localName +
        ".setAttribute(" + attr + ", " + v + "), " + element.localName + "[" + attr + "] ");
    }
    element.removeAttribute(attr);

    if (nonNegative && expectedIdlAttributeResult(v) < 0) {
      try {
        element[attr] = v;
        ok(false, element.localName + "[" + attr + "] = " + v + " should throw IndexSizeError");
      } catch(e) {
        is(e.name, "IndexSizeError", element.localName + "[" + attr + "] = " + v +
          " should throw IndexSizeError");
        is(e.code, DOMException.INDEX_SIZE_ERR, element.localName + "[" + attr + "] = " + v +
          " should throw INDEX_SIZE_ERR");
      }
    } else {
      element[attr] = v;
      if (expectedIdlAttributeResult(v) == -2147483648 && element[attr] == defaultValue) {
        //TBD: Bug 586761: .setAttribute(attr, -2147483648) --> element[attr] == defaultValue instead of -2147483648
        todo_is(element[attr], expectedIdlAttributeResult(v), "Bug 586761: " + element.localName + "[" +
          attr + "] = " + v + ", " + element.localName + "[" + attr + "] ");
      } else {
        is(element[attr], expectedIdlAttributeResult(v), element.localName + "[" + attr + "] = " + v +
          ", " + element.localName + "[" + attr + "] ");
        is(element.getAttribute(attr), expectedIdlAttributeResult(v), element.localName + "[" + attr +
          "] = " + v + ", " + element.localName + ".getAttribute(" + attr + ") ");
      }
    }
    element.removeAttribute(attr);
  });

  // Tests after removeAttribute() is called. Should be equivalent with not set.
  is(element.getAttribute(attr), null,
     "When not set, the content attribute should be null.");
  is(element[attr], defaultValue,
     "When not set, the IDL attribute should return default value.");
}

/**
 * Checks that a given attribute is correctly reflected as a url.
 *
 * @param aParameters   Object    object containing the parameters, which are:
 *  - element           Element   node to test
 *  - attribute         String    name of the attribute
 *     OR
 *    attribute         Object    object containing two attributes, 'content' and 'idl'
 */
function reflectURL(aParameters)
{
  var element = aParameters.element;
  var contentAttr = typeof aParameters.attribute === "string"
                      ? aParameters.attribute : aParameters.attribute.content;
  var idlAttr = typeof aParameters.attribute === "string"
                  ? aParameters.attribute : aParameters.attribute.idl;

  element[idlAttr] = "";
  is(element[idlAttr], document.URL, "Empty string should resolve to document URL");
}
