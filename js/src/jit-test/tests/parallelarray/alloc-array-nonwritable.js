load(libdir + "parallelarray-helpers.js");

function buildSimple()
{
  var subarr = [];
  for (var i = 0; i < 100; i++)
    subarr[i] = 3;
  subarr[100] = 0;

  var expected = [];
  for (var i = 0; i < 256; i++)
    expected[i] = subarr;

  var pa = new ParallelArray([256], function(_) {
    var arrs = [];
    for (var i = 0; i < 100; i++)
      arrs[i] = [0, 1, 2, 3, 4, 5, 6];

    arrs[100] =
      Object.defineProperty([0, 1, 2, 3, 4, 5, 6, 7],
                            "length",
                            { writable: false, value: 7 });

    for (var i = 0; i < 101; i++)
      arrs[i][7] = 7;

    var x = [];
    for (var i = 0; i < 101; i++) {
      var a = arrs[i];
      x[i] = +(a.length === 8) + 2 * +("7" in a);
    }

    return x;
  });

  assertEqParallelArrayArray(pa, expected);
}

if (getBuildConfiguration().parallelJS)
  buildSimple();
