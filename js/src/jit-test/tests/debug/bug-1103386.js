load(libdir + "immutable-prototype.js");

// Random chosen test: js/src/jit-test/tests/auto-regress/bug700295.js
if (globalPrototypeChainIsMutable()) {
  __proto__ = null;
  Object.prototype.__proto__ = this;
}

// Random chosen test: js/src/jit-test/tests/debug/Memory-takeCensus-05.js
Debugger(newGlobal()).memory.takeCensus();
