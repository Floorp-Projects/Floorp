var BUGNUMBER = 1466387;
var summary = 'grayRoot() testing on worker thread';

// Before bug 1466387, the gray root tracer was not set up on worker threads,
// but the grayRoot() function was still available. This resulted in a GC
// collecting the gray root, then a read barrier firing on the dead object.
//
// This is a crashtest. If it does not crash, it will throw a ReferenceError,
// but that's ok.

print('BUGNUMBER: ' + BUGNUMBER);
print("STATUS: " + summary);

if (typeof 'evalInWorder' == 'function') {
    evalInWorker(`
  var wm = new WeakMap();
  grayRoot().map = wm;
  gczeal(4,10);
  evaluate(\`
  grayRoot().map = __v_1173;
  if (!class i   { constructor() { } }  ()) {
    (function __f_252( get       ,    )  {})();
  }
  \`);
`);
}

this.reportCompare && reportCompare(true, true, summary);
