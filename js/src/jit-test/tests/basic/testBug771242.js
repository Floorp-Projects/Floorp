a = function() {
  b = newGlobal()
};
c = [0, 0]
c.sort(a)
function d() {
  yield arguments[4]
}
b.iterate = d
f = Proxy.create(b)
e = Iterator(f, true)
for (p in f) {
  e.next()
}
