function foo() {
    if (arguments[0]) {
        return arguments[1];
    }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
    foo(true, 1);
}

for (var i = 0; i < 100; i++) {
    foo(false);
}
