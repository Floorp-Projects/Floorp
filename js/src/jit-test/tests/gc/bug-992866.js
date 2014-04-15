if (!getBuildConfiguration().parallelJS)
  quit(0);

test([0], "map", function (... Object)  {
  var x = [];
  for (var i = 0; i < 10; i++) {
      x[i] = {};
  }
});

function test(arr, op, func) {
  for (var i = 0; i < 1000; i++) {
    arr[op + "Par"].apply(arr, [func, undefined]);
  }
}
