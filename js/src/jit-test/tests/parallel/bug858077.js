if (getBuildConfiguration().parallelJS) {
  var a = new ParallelArray([1,2,3,4]);
  for (var i = 0; i < 9 && i > -1000; i-- )
    a[i] += [0];
}
