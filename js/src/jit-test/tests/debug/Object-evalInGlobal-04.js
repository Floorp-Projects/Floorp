// Debugger.Object.prototype.evalInGlobal: nested evals

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

assertEq(gw.evalInGlobal("eval('\"Awake\"');").return, "Awake");

// Evaluating non-strict-mode code uses the given global as its variable
// environment.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.evalInGlobal("eval('var x = \"A Brief History of Love\"');\n"
                         + "var y = 'Merriweather Post Pavilion';"
                         + "x;").return,
         "A Brief History of Love");
assertEq(g.x, "A Brief History of Love");
assertEq(g.y, "Merriweather Post Pavilion");

// As above, but notice that we still create bindings on the global, even
// when we've interposed a new environment via 'withBindings'.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.evalInGlobalWithBindings("eval('var x = d1;'); var y = d2; x;",
                                     { d1: "A Brief History of Love",
                                       d2: "Merriweather Post Pavilion" }).return,
         "A Brief History of Love");
assertEq(g.x, "A Brief History of Love");
assertEq(g.y, "Merriweather Post Pavilion");


// Strict mode code variants of the above:

// Evaluating strict-mode code uses a fresh call object as its variable environment.
// Also, calls to eval from strict-mode code run the eval code in a fresh
// call object.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.evalInGlobal("\'use strict\';\n"
                         + "eval('var x = \"A Brief History of Love\"');\n"
                         + "var y = \"Merriweather Post Pavilion\";"
                         + "x;").return,
         "Swing Lo Magellan");
assertEq(g.x, "Swing Lo Magellan");
assertEq(g.y, "The Milk-Eyed Mender");

// Introducing a bindings object shouldn't change this behavior.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.evalInGlobalWithBindings("'use strict'; eval('var x = d1;'); var y = d2; x;",
                                     { d1: "A Brief History of Love",
                                       d2: "Merriweather Post Pavilion" }).return,
         "Swing Lo Magellan");
assertEq(g.x, "Swing Lo Magellan");
assertEq(g.y, "The Milk-Eyed Mender");
