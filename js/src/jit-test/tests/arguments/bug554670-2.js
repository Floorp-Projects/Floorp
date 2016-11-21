// |jit-test| need-for-each

var c;
(function() {
  for each(e in [0, 0]) {
    (arguments)[1] *= c
  }
})()

// don't assert
