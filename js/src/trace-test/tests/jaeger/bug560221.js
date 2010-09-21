try {
  (function() {
    (Object.defineProperty(this, "x", ({
      set: function() {}
    })))
  })()
} catch(e) {}
for (var a = 0; a < 4; a++) {
  x = 7
}

/* Don't bogus assert. */

