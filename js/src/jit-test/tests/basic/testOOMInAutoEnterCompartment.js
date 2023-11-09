// |jit-test| slow; skip-if: (getBuildConfiguration("asan") && getBuildConfiguration("debug"))
// This test is too slow to run at all with ASan in a debug configuration

function fatty() {
    try {
        fatty();
    } catch (e) {
        foo();
    }
}


foo = evalcx("(function foo() { foo.bar() })");
foo.bar = evalcx("(function bar() {})");

fatty();

