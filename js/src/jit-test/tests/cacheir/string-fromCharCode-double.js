// Test inlining String.fromCharCode with Double instead of Int32 inputs.

function test() {
  for (let i = 0; i < 0xffff; ++i) {
    let expected = String.fromCharCode(i);

    // Ensure the Double input is truncated to Int32.
    assertEq(String.fromCharCode(i + 0.5), expected);

    // The input is converted using ToUint16. 
    assertEq(String.fromCharCode(i + 0.5 + 0x10_000), expected);

    // Test ToUint16 conversion when adding INT32_MAX + 1.
    assertEq(String.fromCharCode(i + 0x8000_0000), expected);
  }
}
for (let i = 0; i < 2; ++i) test();
