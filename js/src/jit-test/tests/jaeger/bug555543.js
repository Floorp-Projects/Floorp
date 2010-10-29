(function() {
  for each(let z in [new String(''), new String('q'), new String('')]) {
    if (uneval() < z) (function(){})
  }
})()

/* Don't assert/crash. */

