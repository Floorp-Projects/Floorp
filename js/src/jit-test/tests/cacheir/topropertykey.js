setJitCompilerOption("ion.forceinlineCaches", 1);

function testInt32() {
  var xs = [0, 0];
  var a = [0];

  for (var i = 0; i < 20; ++i) {
      var key = xs[i & 1];
      assertEq(a[key]++, i);
  }
}
for (var i = 0; i < 2; ++i) testInt32();

function testStringInt32() {
  var xs = ["0", "0"];
  var a = [0];

  for (var i = 0; i < 20; ++i) {
      var key = xs[i & 1];
      assertEq(a[key]++, i);
  }
}
for (var i = 0; i < 2; ++i) testStringInt32();

function testString() {
  var xs = ["p", "p"];
  var a = {
    p: 0,
  };

  for (var i = 0; i < 20; ++i) {
      var key = xs[i & 1];
      assertEq(a[key]++, i);
  }
}
for (var i = 0; i < 2; ++i) testString();
