foo = evalcx("(function foo() { foo.bar() })");
foo.bar = evalcx("(function bar() {})");

function fatty() {
    try {
        fatty();
    } catch (e) {
        foo();
    }
}

fatty();
