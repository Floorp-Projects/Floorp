function testMegamorphicLoadSlot(i) {
  var xs = [
    {p: 0},
    {a: 0, p: 1},
    {a: 0, b: 0, p: 2},
    {a: 0, b: 0, c: 0, p: 3},
    {a: 0, b: 0, c: 0, d: 0, p: 4},
    {a: 0, b: 0, c: 0, d: 0, e: 0, p: 5},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, p: 6},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, p: 7},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, p: 8},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, p: 9},
  ];
  var called = 0;
  var obj = {
    get p() {
      called++;
    }
  };

  for (var j = 0; j <= 100; ++j) {
    // Don't use if-statements to avoid cold code bailouts.
    var x = xs[j % 10];
    var y = [x, obj][(i === 1 && j === 100)|0];

    // Can't DCE this instruction.
    y.p;
  }

  assertEq(i === 0 || called === 1, true);
}
for (var i = 0; i < 2; ++i) testMegamorphicLoadSlot(i);

function testMegamorphicLoadSlotByValue(i) {
  var xs = [
    {p: 0},
    {a: 0, p: 1},
    {a: 0, b: 0, p: 2},
    {a: 0, b: 0, c: 0, p: 3},
    {a: 0, b: 0, c: 0, d: 0, p: 4},
    {a: 0, b: 0, c: 0, d: 0, e: 0, p: 5},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, p: 6},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, p: 7},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, p: 8},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, p: 9},
  ];
  var called = 0;
  var obj = {
    get p() {
      called++;
    }
  };

  var p = "p";
  for (var j = 0; j <= 100; ++j) {
    // Don't use if-statements to avoid cold code bailouts.
    var x = xs[j % 10];
    var y = [x, obj][(i === 1 && j === 100)|0];

    // Can't DCE this instruction.
    y[p];
  }

  assertEq(i === 0 || called === 1, true);
}
for (var i = 0; i < 2; ++i) testMegamorphicLoadSlotByValue(i);

function testMegamorphicHasProp(i) {
  var xs = [
    {p: 0},
    {a: 0, p: 1},
    {a: 0, b: 0, p: 2},
    {a: 0, b: 0, c: 0, p: 3},
    {a: 0, b: 0, c: 0, d: 0, p: 4},
    {a: 0, b: 0, c: 0, d: 0, e: 0, p: 5},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, p: 6},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, p: 7},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, p: 8},
    {a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, p: 9},
  ];
  var called = 0;
  var obj = new Proxy({}, {
    has() {
      called++;
    }
  });

  for (var j = 0; j <= 100; ++j) {
    // Don't use if-statements to avoid cold code bailouts.
    var x = xs[j % 10];
    var y = [x, obj][(i === 1 && j === 100)|0];

    // Can't DCE this instruction.
    "p" in y;
  }

  assertEq(i === 0 || called === 1, true);
}
for (var i = 0; i < 2; ++i) testMegamorphicHasProp(i);
