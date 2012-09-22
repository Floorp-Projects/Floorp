function f1(g=((function () { return 4; }) for (x of [1]))) {
  return g.next()();
}
assertEq(f1(), 4);
