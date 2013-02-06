// Binary: cache/js-dbg-64-940078281bbf-linux
// Flags: --ion-eager
//
function List(l) {
  this.l = l;
}
function f(p) {
  return g(p.l);
};
function g(p) {
  return !(p instanceof List) ? null : f(p.l);
};
list =
  new List(new List(
    new List(new List(
      new List(new List(
        new List(new List(null))))))))
for (let i = 0; i < 99999; i++) {
  g(list);
}
