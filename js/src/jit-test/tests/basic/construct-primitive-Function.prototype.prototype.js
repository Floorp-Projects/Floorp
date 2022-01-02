Object.defineProperty(Function.prototype, "prototype", {
  get: function() { throw 17; },
  set: function() { throw 42; }
});
this.hasOwnProperty("Intl");
