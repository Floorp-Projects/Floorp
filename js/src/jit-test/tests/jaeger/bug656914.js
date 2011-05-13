try {
  new MyObject;
} catch (e) {}

function MyObject() {
  return;
  return this;
}

function Foo(x) {
  if (x)
    this.f = x;
}
var q = new Foo(false);
for (a in q) { assertEq(true, false); }

function Bar(x, y) {
  if (!x)
    return;
  this.f = y;
}
var q2 = new Bar(false, true);
for (b in q2) { assertEq(true, false); }
