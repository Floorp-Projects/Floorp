
function Foo(x)
{
  this.f = x + 10;
}

var x = new Foo(0);
assertEq(10, eval("x.f"));

called = false;
Object.defineProperty(Foo.prototype, 'f', {set: function() { called = true; }});

var y = new Foo(0);
assertEq(10, eval("x.f"));
assertEq(undefined, eval("y.f"));
assertEq(called, true);
