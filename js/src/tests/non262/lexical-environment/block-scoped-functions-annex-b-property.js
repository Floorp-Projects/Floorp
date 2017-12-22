// |reftest| skip-if(!xulRuntime.shell)

// Define a global getter without a setter.
Object.defineProperty(this, "x", {
  get: function () { return "get-x"; },
  configurable: true
});

// Simulate loading a 2nd script with evaluate, else we would DEFVAR the x and
// the above defineProperty would fail in trying to redefine a non-configurable
// property on the global.
evaluate(`{
  function x() { return "fun-x"; }
}`);

// Annex B is supposed to be like an assignment. Should not blow away the
// existing setter-less getter.
reportCompare(x, "get-x");
