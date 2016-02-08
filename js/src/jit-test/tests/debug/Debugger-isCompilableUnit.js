load(libdir + "asserts.js");

const bad_types = [
    2112,
    {geddy: "lee"},
    () => 1,
    [],
    Array
]

// We only accept strings around here!
for (var badType of bad_types) {
    assertThrowsInstanceOf(() => {
        Debugger.isCompilableUnit(badType);
    }, TypeError);
}

const compilable_units = [
   "wubba-lubba-dub-dub",
   "'Get Schwifty!'",
   "1 + 2",
   "function f(x) {}",
   "function x(f,) {", // statements with bad syntax are always compilable
   "let x = 100",
   ";;;;;;;;",
   "",
   " ",
   "\n",
   "let x",
]

const non_compilable_units = [
    "function f(x) {",
    "(...d) =>",
    "{geddy:",
    "{",
    "[1, 2",
    "[",
    "1 +",
    "let x =",
    "3 ==",
]

for (var code of compilable_units) {
    assertEq(Debugger.isCompilableUnit(code), true);
}

for (var code of non_compilable_units) {
    assertEq(Debugger.isCompilableUnit(code), false);
}

// Supplying no arguments should throw a type error
assertThrowsInstanceOf(() => {
    Debugger.isCompilableUnit();
}, TypeError);

// Supplying extra arguments should be fine
assertEq(Debugger.isCompilableUnit("", 1, 2, 3, 4, {}, []), true);
