var b = new ArrayBuffer(4);
var dv = new DataView(b);
dv.setInt32(0, 42);
var w = wrap(dv);
assertEq(DataView.prototype.getInt32.call(w, 0), 42);
