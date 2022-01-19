// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
ignoreUnhandledRejections();

Object.defineProperty(Promise, Symbol.species, {
  value: function(g) {
    g(function() {}, function() {})
  }
});
new ReadableStream().tee();
