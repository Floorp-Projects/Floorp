a = {}
o14 = [].__proto__
function g(o) {
  o.f = o14
}
for (let i = 0; i < 50; i++) {
  g(a)
}
