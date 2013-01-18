// Binary: cache/js-dbg-64-b4da2d439cbc-linux
// Flags: -m -n -a
//

function f() {
  var oa = [];
  for (var i = 0; i < 8; ++i) {
    var o = {};
    oa[(new Int32Array(ArrayBuffer.prototype).length)] = o;
  }
}
f();
