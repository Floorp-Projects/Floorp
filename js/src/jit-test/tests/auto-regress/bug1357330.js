function byteValue(value) {
  var isLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] !== 0;
  var ui8 = new Uint8Array(new Float64Array([value]).buffer);

  var hex = "0123456789ABCDEF";
  var s = "";
  for (var i = 0; i < 8; ++i) {
    var v = ui8[isLittleEndian ? 7 - i : i];
    s += hex[(v >> 4) & 0xf] + hex[v & 0xf];
  }
  return s;
}

var obj = {};
Object.defineProperty(obj, "prop", {value: NaN});
Object.defineProperty(obj, "prop", {value: -NaN});
assertEq(byteValue(obj.prop), byteValue(NaN));
