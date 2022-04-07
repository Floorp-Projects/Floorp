function toCharCodes(str) {
  return [...str].map(s => s.length === 1 ? [s.charCodeAt(0)] : [s.charCodeAt(0), s.charCodeAt(1)])
                 .flat();
}

function test() {
  // [Ascii; Latin-1, non-Ascii; non-Latin-1; non-BMP]
  const constant = "AaÁáĀā𝐀𝐚";
  const charCodes = toCharCodes(constant);

  // Create a linear, but non-constant string with the same contents.
  const linear = String.fromCharCode(...charCodes);
  assertEq(linear, constant);

  // Optimisation applies for |MCharCodeAt(MFromCharCode(MCharCodeAt(str, idx)), 0|.
  for (let i = 0; i < 500; ++i) {
    let idx = i & 3;

    // str[idx] is compiled to |MFromCharCode(MCharCodeAt(str, idx))|.
    assertEq(constant[idx].charCodeAt(0), charCodes[idx]);
    assertEq(linear[idx].charCodeAt(0), charCodes[idx]);

    // str.charAt(idx) is compiled to |MFromCharCode(MCharCodeAt(str, idx))|.
    assertEq(constant.charAt(idx).charCodeAt(0), charCodes[idx]);
    assertEq(linear.charAt(idx).charCodeAt(0), charCodes[idx]);
  }
}
for (let i = 0; i < 4; ++i) {
  test();
}

function testNonConstantIndex() {
  // [Ascii; Latin-1, non-Ascii; non-Latin-1; non-BMP]
  const constant = "AaÁáĀā𝐀𝐚";
  const charCodes = toCharCodes(constant);

  // Create a linear, but non-constant string with the same contents.
  const linear = String.fromCharCode(...charCodes);
  assertEq(linear, constant);

  // No optimisation when the index isn't a constant zero.
  let indices = [0, 1, 2, 3];
  for (let i = 0; i < 500; ++i) {
    let idx = i & 3;

    // Always zero, but too complicated to infer at compile time.
    let zero = indices[idx] - idx;

    assertEq(constant[idx].charCodeAt(zero), charCodes[idx]);
    assertEq(linear[idx].charCodeAt(zero), charCodes[idx]);

    assertEq(constant.charAt(idx).charCodeAt(zero), charCodes[idx]);
    assertEq(linear.charAt(idx).charCodeAt(zero), charCodes[idx]);
  }
}
for (let i = 0; i < 4; ++i) {
  testNonConstantIndex();
}

function testOOB() {
  // [Ascii; Latin-1, non-Ascii; non-Latin-1; non-BMP]
  const constant = "AaÁáĀā𝐀𝐚";
  const charCodes = toCharCodes(constant);

  // Create a linear, but non-constant string with the same contents.
  const linear = String.fromCharCode(...charCodes);
  assertEq(linear, constant);

  // No optimisation when the index is out-of-bounds.
  for (let i = 0; i < 500; ++i) {
    let idx = i & 3;

    assertEq(constant[idx].charCodeAt(1), NaN);
    assertEq(linear[idx].charCodeAt(1), NaN);

    assertEq(constant.charAt(idx).charCodeAt(1), NaN);
    assertEq(linear.charAt(idx).charCodeAt(1), NaN);
  }
}
for (let i = 0; i < 4; ++i) {
  testOOB();
}
