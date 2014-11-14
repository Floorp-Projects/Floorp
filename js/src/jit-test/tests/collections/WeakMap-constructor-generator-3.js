// The argument to WeakMap may be a generator-iterator that produces no values.

new WeakMap(x for (x of []));

function none() {
    if (0) yield 0;
}
new WeakMap(none());

function* none2() {
    if (0) yield 0;
}
new WeakMap(none2());
