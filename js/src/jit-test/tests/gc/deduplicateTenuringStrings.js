// |jit-test| skip-if: !('stringRepresentation' in this)

// This is to test the correctness of the string deduplication algorithm during
// the tenuring phase. Same strings below refer to the same character encoding
// (either latin1 or twobyte) and the same characters.

// Tests:
// 1. Same strings with same flags and zones should be deduplicated for
// all linear strings except atoms and external strings.
// 2. Same strings, but from different zones should not be deduplicated.
// 3. Same strings, but with different flags should not be deduplicated.

// We require predictable GC timing to make sure the correct
// strings are tenured together.
gczeal(0);

var helperCode = `
function makeInlineStr(isLatin1) {
  var s = isLatin1 ? "123456789*1" : "一二三";
  return s + s;
}

// Generic linear strings are non-atom, non-extensible, non-inline
// linear strings.
// Generic linear strings can only have latin1 characters.
function makeGenericLinearStr() {
  return notes(() => 1);
}

function makeRopeStr(isLatin1) {
  var left = isLatin1 ? "1" : "一";
  var right = isLatin1 ? "123456789*123456789*123456" :
    					   "一二三四五六七八九*一二三四五六七八";
  return left + right;
}

function makeExtensibleStr(isLatin1) {
  var r = makeRopeStr(isLatin1);
  ensureLinearString(r);
  return r;
}

function makeExtensibleStrFrom(str) {
  var left = str.substr(0, str.length/2);
  var right = str.substr(str.length/2, str.length);
  var ropeStr = left + right;
  return ensureLinearString(ropeStr);
}

function makeDependentStr(isLatin1) {
  var e = makeExtensibleStr(isLatin1);
  var r1 = e + "!";
  var r2 = e + r1;
  ensureLinearString(r2);
  return r1;
}

function makeDependentStrFrom(str) {
  var e = makeExtensibleStrFrom(str);
  var r1 = e.substr(0, e.length/2) + e.substr(e.length/2, e.length);
  var r2 = e + r1;
  ensureLinearString(r2);
  return r1;
}

function makeExternalStr(isLatin1) {
  return isLatin1 ? newString("12345678", {external: true}) :
                    newString("一二三", {external: true});
}

function tenureStringsWithSameChars(str1, str2, isDeduplicatable) {
  minorgc();
  assertEq(stringRepresentation(str1) == stringRepresentation(str2),
	   isDeduplicatable);
}

function assertDiffStrRepAfterMinorGC(g1, g2) {
  minorgc();
  g1.eval(\`strRep = stringRepresentation(str);\`);
  g2.eval(\`strRep = stringRepresentation(str);\`);
  assertEq(g1.strRep == g2.strRep, false);
}
`;

eval(helperCode);

// test1:
// Same strings with same flags and zones should be deduplicated for all linear
// strings except atoms, external strings.
function test1(isLatin1) {
  const isDeduplicatable = true;

  // Deduplicatable:
  // --> Inline Strings
  var str1 = makeInlineStr(isLatin1);
  var str2 = makeInlineStr(isLatin1);
  tenureStringsWithSameChars(str1, str2, isDeduplicatable);

  // --> Extensible Strings
  str1 = makeExtensibleStr(isLatin1);
  str2 = makeExtensibleStr(isLatin1);
  tenureStringsWithSameChars(str1, str2, isDeduplicatable);

  // --> Dependent Strings
  str1 = makeDependentStr(isLatin1);
  str2 = makeDependentStr(isLatin1);
  tenureStringsWithSameChars(str1, str2, isDeduplicatable);

  // --> Generic Linear Strings
  if (isLatin1) {
    var str1 = makeGenericLinearStr();
    var str2 = makeGenericLinearStr();
    tenureStringsWithSameChars(str1, str2, isDeduplicatable);
  }

  // Non-Deduplicatable:
  // --> Rope Strings
  str1 = makeRopeStr(isLatin1);
  str2 = makeRopeStr(isLatin1);
  tenureStringsWithSameChars(str1, str2, !isDeduplicatable);

  // --> Atom strings are deduplicated already but not through string
  // deduplication during tenuring.

  // --> External strings are not nursery allocated.
}

// test2:
// Same strings, but from different zones should not be deduplicated.
function test2(isLatin1) {
  var g1 = newGlobal({ newCompartment: true });
  var g2 = newGlobal({ newCompartment: true });

  g1.eval(helperCode);
  g2.eval(helperCode);

  // --> Inline Strings
  g1.eval(`var str = makeInlineStr(${isLatin1}); `);
  g2.eval(`var str = makeInlineStr(${isLatin1}); `);
  assertDiffStrRepAfterMinorGC(g1, g2);

  // --> Extensible Strings
  g1.eval(`str = makeExtensibleStr(${isLatin1}); `);
  g2.eval(`str = makeExtensibleStr(${isLatin1}); `);
  assertDiffStrRepAfterMinorGC(g1, g2);

  // --> Dependent Strings
  g1.eval(`str = makeDependentStr(${isLatin1}); `);
  g2.eval(`str = makeDependentStr(${isLatin1}); `);
  assertDiffStrRepAfterMinorGC(g1, g2);

  // --> Generic Linear Strings
  if (isLatin1) {
    g1.eval(`str = makeGenericLinearStr();`);
    g2.eval(`str = makeGenericLinearStr();`);
    assertDiffStrRepAfterMinorGC(g1, g2);
  }
}

// test3:
// Same strings, but with different flags should not be deduplicated.
function test3(isLatin1) {
  const isDeduplicatable = true;

  // --> Dependent String and Extensible String
  var dependentStr = makeDependentStr(isLatin1);
  var extensibleStr = makeExtensibleStrFrom(dependentStr);
  tenureStringsWithSameChars(dependentStr, extensibleStr, !isDeduplicatable);

  if (isLatin1) {
    // --> Generic Linear String and Extensible String
    var genericLinearStr = makeGenericLinearStr();
    var extensibleStr = makeExtensibleStrFrom(genericLinearStr);
    tenureStringsWithSameChars(
      genericLinearStr,
      extensibleStr,
      !isDeduplicatable
    );

    // --> Generic Linear String and Dependent String
    var dependentStr = makeDependentStrFrom(genericLinearStr);
    tenureStringsWithSameChars(
      dependentStr,
      genericLinearStr,
      !isDeduplicatable
    );
  }

  // --> Inline strings are too short to have the same chars as the extensible
  // strings, generic linear strings and dependent strings
}

function runTests() {
  var charEncoding = { TWOBYTE: 0, LATIN1: 1 };

  test1(charEncoding.TWOBYTE);
  test2(charEncoding.TWOBYTE);
  test3(charEncoding.TWOBYTE);

  test1(charEncoding.LATIN1);
  test2(charEncoding.LATIN1);
  test3(charEncoding.LATIN1);
}

runTests();
