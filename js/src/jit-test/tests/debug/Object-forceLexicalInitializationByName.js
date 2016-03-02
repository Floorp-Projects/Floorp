load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

let errorOne, errorTwo;

function evalErrorStr(global, evalString) {
    try {
        global.evaluate(evalString);
        return undefined;
    } catch (e) {
        return e.toString();
    }
}


assertEq(evalErrorStr(g, "let y = IDONTEXIST;"), "ReferenceError: IDONTEXIST is not defined");
assertEq(evalErrorStr(g, "y = 1;"),
         "ReferenceError: can't access lexical declaration `y' before initialization");

// Here we flip the uninitialized binding to undfined.
assertEq(gw.forceLexicalInitializationByName("y"), true);
assertEq(g.evaluate("y"), undefined);
g.evaluate("y = 1;");
assertEq(g.evaluate("y"), 1);

// Ensure that bogus bindings return false, but otherwise trigger no error or
// side effect.
assertEq(gw.forceLexicalInitializationByName("idontexist"), false);
assertEq(evalErrorStr(g, "idontexist"), "ReferenceError: idontexist is not defined");

// Ensure that ropes (non-atoms) behave properly
assertEq(gw.forceLexicalInitializationByName(("foo" + "bar" + "bop" + "zopple" + 2 + 3).slice(1)),
                                             false);
assertEq(evalErrorStr(g, "let oobarbopzopple23 = IDONTEXIST;"), "ReferenceError: IDONTEXIST is not defined");
assertEq(gw.forceLexicalInitializationByName(("foo" + "bar" + "bop" + "zopple" + 2 + 3).slice(1)),
                                             true);
assertEq(g.evaluate("oobarbopzopple23"), undefined);

// Ensure that only strings are accepted by forceLexicalInitializationByName
const bad_types = [
    2112,
    {geddy: "lee"},
    () => 1,
    [],
    Array,
    "'1'", // non-identifier
]

for (var badType of bad_types) {
    assertThrowsInstanceOf(() => {
        gw.forceLexicalInitializationByName(badType);
    }, TypeError);
}

// Finally, supplying no arguments should throw a type error
assertThrowsInstanceOf(() => {
    Debugger.isCompilableUnit();
}, TypeError);
