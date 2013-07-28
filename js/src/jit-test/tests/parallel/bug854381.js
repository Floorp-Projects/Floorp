function bug854381() {
  // Don't crash.
  function toString(r) {
    var l = 2;
    var result = "";
    for (var i = 0; i < l; i++)
      result += r.get(i);
    return result;
  }

  var p = new ParallelArray(['x', 'x']);
  var r = new ParallelArray([toString(p), 42]);

  gc();
  print(toString(r));
}

if (getBuildConfiguration().parallelJS) {
  bug854381();
}
