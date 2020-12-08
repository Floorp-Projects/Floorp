load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
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
         "ReferenceError: can't access lexical declaration 'y' before initialization");

const LINEAR_SEARCHES_MAX = 3;
const SHAPE_CACHE_MIN_ENTRIES = 3;

// Give the lexical enough properties so that it isBigEnoughForAShapeTable().
for (i in [...Array(SHAPE_CACHE_MIN_ENTRIES)])
    gw.executeInGlobal(`let x${i} = 1`);

// Search for y just enough times to cause the next search to trigger
// Shape::cachify().
for (i in [...Array(LINEAR_SEARCHES_MAX - 1)])
    gw.executeInGlobal("y");

// Here we flip the uninitialized binding to undefined. But in the process, we
// will do the lookup on y that will trigger Shape::cachify. Verify that it
// happens in the correct compartment.
assertEq(gw.forceLexicalInitializationByName("y"), true);
