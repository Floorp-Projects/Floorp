// Make sure new script properties can be invalidated on specialized prototype
// types while they are still being constructed.

function Foo(a, b, c) {
  this.x = a + b;
  this.y = c;
}

updated = false;
var o = {valueOf: function() {
    Object.defineProperty(Object.prototype, 'y', {set:function() { updated = true; }})
  }};

function Bar() {}
Bar.prototype = new Foo(o, 1, 2);
assertEq(updated, true);
assertEq(Bar.prototype.y, undefined);
