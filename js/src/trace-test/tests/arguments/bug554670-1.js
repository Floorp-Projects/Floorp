x = true;
(function() {
  for each(let c in [0, x]) {
    (arguments)[4] *= c
  }
})()

// don't assert
