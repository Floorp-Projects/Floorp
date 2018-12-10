Object.defineProperty(Promise, Symbol.species, {
  value: function(g) {
    g(function() {}, function() {})
  }
});
new ReadableStream().tee();
