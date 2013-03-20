load(libdir + "parallelarray-helpers.js");
testFilter(range(0, 1024), function(e, i) {
  return (i % 3) != 0;
});

