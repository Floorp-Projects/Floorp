/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * reflect.js is a collection of methods to test HTML attribute reflection.
 * Each of attribute is reflected differently, depending on various parameters,
 * see:
 * http://www.whatwg.org/html/#reflecting-content-attributes-in-idl-attributes
 *
 * Do not forget to add these line at the beginning of each new reflect* method:
 * ok(aAttr in aElement, aAttr + " should be an IDL attribute of this element");
 * is(typeof aElement[aAttr], <type>, aAttr + " IDL attribute should be a <type>");
 */

/**
 * Checks that a given attribute is correctly reflected as a string.
 *
 * @param aElement      Element   node to test
 * @param aAttr         String    name of the attribute
 * @param aOtherValues  Array     other values to test in addition of the default ones [optional]
 */
function reflectString(aElement, aAttr, aOtherValues)
{
  var otherValues = aOtherValues !== undefined ? aOtherValues : [];

  ok(aAttr in aElement, aAttr + " should be an IDL attribute of this element");
  is(typeof aElement[aAttr], "string", aAttr + " IDL attribute should be a string");

  // Tests when the attribute isn't set.
  is(aElement.getAttribute(aAttr), null,
     "When not set, the content attribute should be undefined.");
  is(aElement[aAttr], "",
     "When not set, the IDL attribute should return the empty string");

  /**
   * TODO: as long as null stringification doesn't fallow the webidl specs,
   * don't add it to the loop below and keep it here.
   */
  aElement.setAttribute(aAttr, null);
  todo_is(aElement.getAttribute(aAttr), "null",
     "null should have been stringified to 'null'");
  todo_is(aElement[aAttr], "null",
     "null should have been stringified to 'null'");
  aElement.removeAttribute(aAttr);

  aElement[aAttr] = null;
  todo_is(aElement.getAttribute(aAttr), "null",
     "null should have been stringified to 'null'");
  todo_is(aElement[aAttr], "null",
     "null should have been stringified to 'null'");
  aElement.removeAttribute(aAttr);

  // Tests various strings.
  var stringsToTest = [
    // [ test value, expected result ]
    [ "", "" ],
    [ "null", "null" ],
    [ "undefined", "undefined" ],
    [ "foo", "foo" ],
    [ aAttr, aAttr ],
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
    aElement.setAttribute(aAttr, v);
    is(aElement[aAttr], r,
       "IDL attribute should return the value it has been set to.");
    is(aElement.getAttribute(aAttr), r,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v;
    is(aElement[aAttr], r,
       "IDL attribute should return the value it has been set to.");
    is(aElement.getAttribute(aAttr), r,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);
  });

  // Tests after removeAttribute() is called. Should be equivalent with not set.
  is(aElement.getAttribute(aAttr), null,
     "When not set, the content attribute should be undefined.");
  is(aElement[aAttr], "",
     "When not set, the IDL attribute should return the empty string");
}

/**
 * Checks that a given attribute name for a given element is correctly reflected
 * as an unsigned int.
 */
function reflectUnsignedInt(aElement, aAttr, aNonNull, aDefault)
{
  function checkGetter(aElement, aAttr, aValue)
  {
    is(aElement[aAttr], aValue, "." + aAttr + " should be equals " + aValue);
    is(aElement.getAttribute(aAttr), aValue,
       "@" + aAttr + " should be equals " + aValue);
  }

  if (!aDefault) {
    if (aNonNull) {
      aDefault = 1;
    } else {
      aDefault = 0;
    }
  }

  ok(aAttr in aElement, aAttr + " should be an IDL attribute of this element");
  is(typeof aElement[aAttr], "number", aAttr + " IDL attribute should be a string");

  // Check default value.
  is(aElement[aAttr], aDefault, "default value should be " + aDefault);
  ok(!aElement.hasAttribute(aAttr), aAttr + " shouldn't be present");

  var values = [ 1, 3, 42, 2147483647 ];

  for each (var value in values) {
    aElement[aAttr] = value;
    checkGetter(aElement, aAttr, value);
  }

  for each (var value in values) {
    aElement.setAttribute(aAttr, value);
    checkGetter(aElement, aAttr, value);
  }

  // -3000000000 is equivalent to 1294967296 when using the IDL attribute.
  aElement[aAttr] = -3000000000;
  checkGetter(aElement, aAttr, 1294967296);
  // When setting the content atribute, it's a string so it will be unvalid.
  aElement.setAttribute(aAttr, -3000000000);
  is(aElement.getAttribute(aAttr), -3000000000,
     "@" + aAttr + " should be equals to " + -3000000000);
  is(aElement[aAttr], aDefault,
     "." + aAttr + " should be equals to " + aDefault);

  var nonValidValues = [
    /* invalid value, value in the unsigned int range */
    [ -2147483648, 2147483648 ],
    [ -1,          4294967295 ],
    [ 3147483647,  3147483647 ],
  ];

  for each (var values in nonValidValues) {
    aElement[aAttr] = values[0];
    is(aElement.getAttribute(aAttr), values[1],
       "@" + aAttr + " should be equals to " + values[1]);
    is(aElement[aAttr], aDefault,
       "." + aAttr + " should be equals to " + aDefault);
  }

  for each (var values in nonValidValues) {
    aElement.setAttribute(aAttr, values[0]);
    is(aElement.getAttribute(aAttr), values[0],
       "@" + aAttr + " should be equals to " + values[0]);
    is(aElement[aAttr], aDefault,
       "." + aAttr + " should be equals to " + aDefault);
  }

  // Setting to 0 should throw an error if aNonNull is true.
  var caught = false;
  try {
    aElement[aAttr] = 0;
  } catch(e) {
    caught = true;
    is(e.code, DOMException.INDEX_SIZE_ERR, "exception should be INDEX_SIZE_ERR");
  }

  if (aNonNull) {
    ok(caught, "an exception should have been caught");
  } else {
    ok(!caught, "no exception should have been caught");
  }

  // If 0 is set in @aAttr, it will be ignored when calling .aAttr.
  aElement.setAttribute(aAttr, 0);
  is(aElement.getAttribute(aAttr), 0, "@" + aAttr + " should be equals to 0");
  if (aNonNull) {
    is(aElement[aAttr], aDefault,
       "." + aAttr + " should be equals to " + aDefault);
  } else {
    is(aElement[aAttr], 0, "." + aAttr + " should be equals to 0");
  }
}

/**
 * @param aElement            Element     node to test on
 * @param aAttr               String      name of the attribute
 * @param aValidValues        Array       valid values we support
 * @param aInvalidValues      Array       invalid values
 * @param aDefaultValue       String      default value when no valid value is set [optional]
 * @param aUnsupportedValues  Array       valid values we do not support [optional]
 */
function reflectLimitedEnumerated(aElement, aAttr, aValidValues, aInvalidValues,
                                  aDefaultValue, aUnsupportedValues)
{
  var defaultValue = aDefaultValue !== undefined ? aDefaultValue : "";
  var unsupportedValues = aUnsupportedValues !== undefined ? aUnsupportedValues
                                                           : [];

  ok(aAttr in aElement, aAttr + " should be an IDL attribute of this element");
  is(typeof aElement[aAttr], "string", aAttr + " IDL attribute should be a string");

  // Explicitly check the default value.
  aElement.removeAttribute(aAttr);
  is(aElement[aAttr], defaultValue,
     "When no attribute is set, the value should be the default value.");

  // Check valid values.
  aValidValues.forEach(function (v) {
    aElement.setAttribute(aAttr, v);
    is(aElement[aAttr], v,
       v + " should be accepted as a valid value for " + aAttr);
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);

    aElement.setAttribute(aAttr, v.toUpperCase());
    is(aElement[aAttr], v,
       "Enumerated attributes should be case-insensitive.");
    is(aElement.getAttribute(aAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v;
    is(aElement[aAttr], v,
       v + " should be accepted as a valid value for " + aAttr);
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v.toUpperCase();
    is(aElement[aAttr], v,
       "Enumerated attributes should be case-insensitive.");
    is(aElement.getAttribute(aAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    aElement.removeAttribute(aAttr);
  });

  // Check invalid values.
  aInvalidValues.forEach(function (v) {
    aElement.setAttribute(aAttr, v);
    is(aElement[aAttr], defaultValue,
       "When the content attribute is set to an invalid value, the default value should be returned.");
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should not have been changed.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v;
    is(aElement[aAttr], defaultValue,
       "When the value is set to an invalid value, the default value should be returned.");
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should not have been changed.");
    aElement.removeAttribute(aAttr);
  });

  // Check valid values we currently do not support.
  // Basically, it's like the checks for the valid values but with some todo's.
  unsupportedValues.forEach(function (v) {
    aElement.setAttribute(aAttr, v);
    todo_is(aElement[aAttr], v,
            v + " should be accepted as a valid value for " + aAttr);
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);

    aElement.setAttribute(aAttr, v.toUpperCase());
    todo_is(aElement[aAttr], v,
            "Enumerated attributes should be case-insensitive.");
    is(aElement.getAttribute(aAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v;
    todo_is(aElement[aAttr], v,
            v + " should be accepted as a valid value for " + aAttr);
    is(aElement.getAttribute(aAttr), v,
       "Content attribute should return the value it has been set to.");
    aElement.removeAttribute(aAttr);

    aElement[aAttr] = v.toUpperCase();
    todo_is(aElement[aAttr], v,
            "Enumerated attributes should be case-insensitive.");
    is(aElement.getAttribute(aAttr), v.toUpperCase(),
       "Content attribute should not be lower-cased.");
    aElement.removeAttribute(aAttr);
  });
}

