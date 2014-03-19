
for (var i = 0; i < 10; i++) {
  x = ArrayBuffer(4)
  x.f = (function() {})
  Uint16Array(x).set(JSON.parse)
  gcslice()
}
