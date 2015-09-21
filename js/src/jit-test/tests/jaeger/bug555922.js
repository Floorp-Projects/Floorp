enableNoSuchMethod();

(function() {
  let(z) {
    for each(b in [{}]) { ({
        get __noSuchMethod__() { return Function }
      }).w()
    }
  }
})()

/* Don't crash/assert. */

