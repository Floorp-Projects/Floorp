// |jit-test| --ion-pgo=on;

target = handler = {}
for (p of[new Proxy(target, handler)])
  evaluate("foo()");
function foo() {
    symbols = [Symbol]
    values = [NaN]
    for (comparator of[""])
        for (b of values) assertEq;
    for (comparator of[""])
        for (a of symbols)
            for (b of values) assertEq;
}
