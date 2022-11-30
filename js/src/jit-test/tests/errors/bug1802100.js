// |jit-test| exitstatus: 3

// Throw an object as an exception from the interrupt handler.
setInterruptCallback(function() {
  throw {};
});

// Cause a second exception while trying to report the first exception.
Object.prototype[Symbol.toPrimitive] = function () { return {} }

// Trigger an interrupt in the prologue of a baseline function.
function b() {
  var c;
  interruptIf(true);
  b();
}
b();
