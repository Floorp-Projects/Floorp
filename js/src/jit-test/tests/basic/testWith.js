
// basic 'with' functionality

var o = {foo: true};
with(o) {
    foo = 10;
}
assertEq(o.foo, 10);
