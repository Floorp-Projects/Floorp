
for (var i = 0; i < 10; i++) {
  x = new ArrayBuffer(4)
  x.f = (function() {})
  new Uint16Array(x).set(JSON.parse)
  gcslice()
}
