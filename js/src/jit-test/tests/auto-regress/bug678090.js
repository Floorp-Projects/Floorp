// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-609f37c36bd7-linux
// Flags: -j -m -a
//

function toSource(arr) {
  for (i=0; i<len; i++) {}
}
test();
function test() {
  function gen() {
    var c = test;
    try {
      yield c;
    } finally {
      this.toSource();
    }
  }
  var iter = gen();
  for (i in iter) {
    500();
  }
}
