load(libdir + 'asserts.js');

// Censor potentially existing toSource to trigger fallback in ValueToSource.
Function.prototype.toSource = null;

assertTypeErrorMessage(() => { new (function*() {}) },
                      "(function*() {}) is not a constructor");
