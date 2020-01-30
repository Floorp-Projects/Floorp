// Tests for JSOp::CallIgnoresRv in optional chains.

// Note:: IgnoresReturnValueNative is supported for Array.prototype.splice.

// Test for optional call.
function testOptionalCall() {
  for (var i = 0; i < 100; ++i) {
    var x = [1, 2, 3];
    x.splice?.(0);
  }
}

for (var i = 0; i < 5; ++i) { testOptionalCall(); }

// Test for optional prop directly followed by call.
function testOptionalProp() {
  for (var i = 0; i < 100; ++i) {
    var x = [1, 2, 3];
    x?.splice(0);
  }
}

for (var i = 0; i < 5; ++i) { testOptionalProp(); }

// Test for call in optional chain expression.
function testOptionalChain() {
  for (var i = 0; i < 100; ++i) {
    var x = [1, 2, 3];
    var o = {x};
    o?.x.splice(0);
  }
}

for (var i = 0; i < 5; ++i) { testOptionalChain(); }
