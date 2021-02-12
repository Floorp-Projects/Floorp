// Constant folding optimizes the element access with string that contains
// 32-bit unsignned integer.
// The result shouldn't be visible to the script, and the optimization shouldn't
// change the behavior.

// Asserts that the property name and the value are same.
var validator = new Proxy({}, {
  set(that, prop, value) {
    assertEq(prop, value);
  }
});

// Optimizable cases.
validator["0"] = "0";
validator["1"] = "1";
validator["10"] = "10";
validator["123"] = "123";

// Not optimizable cases.

// More than UINT32_MAX.
validator["4294967296"] = "4294967296";
validator["10000000000000"] = "10000000000000";

// Leading 0.
validator["01"] = "01";
validator["0000001"] = "0000001";

// Sign.
validator["+1"] = "+1";
validator["-1"] = "-1";

// Non-decimal
validator["0b1"] = "0b1";
validator["0o1"] = "0o1";
validator["0x1"] = "0x1";

// Non-integer.
validator["1.1"] = "1.1";
validator["1."] = "1.";
validator[".1"] = ".1";
validator["0.1"] = "0.1";

// Extra character.
validator["1a"] = "1a";
validator["1 "] = "1 ";
validator[" 1"] = " 1";
