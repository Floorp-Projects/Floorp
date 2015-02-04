// The argument to Map may be a generator-iterator that produces no values.

var m = new Map(x for (x of []));
assertEq(m.size, 0);

function none() {
    if (0) yield 0;
}
m = new Map(none());
assertEq(m.size, 0);
