// Binary: cache/js-dbg-32-a409054e1395-linux
// Flags: -m
//
load(libdir + 'asserts.js');
// value is not iterable
(function() {
  for (var [x]  in [[] < []])
  {
    // Just a useless expression.
    [];
  }
})();
