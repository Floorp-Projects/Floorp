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
