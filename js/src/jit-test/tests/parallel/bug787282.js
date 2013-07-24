// |jit-test| error: TypeError

var protoArr = Proxy.create({}, null);
void (Array.prototype.__proto__ = protoArr);
gczeal(2);
function testCopyBigArray() {
  var a = new Array(10000);
  for (var cnt = 0; cnt < a.length; cnt+=2) {
    var p = new ParallelArray(a);
  }
}

if (getBuildConfiguration().parallelJS)
  testCopyBigArray();
else
  throw new TypeError();
