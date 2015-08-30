// Debugger.Object.prototype.executeInGlobal: nested evals

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

assertEq(gw.executeInGlobal("eval('\"Awake\"');").return, "Awake");

// Evaluating non-strict-mode code uses the given global as its variable
// environment.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.executeInGlobal("eval('var x = \"A Brief History of Love\"');\n"
                            + "var y = 'Merriweather Post Pavilion';"
                            + "x;").return,
         "A Brief History of Love");
assertEq(g.x, "A Brief History of Love");
assertEq(g.y, "Merriweather Post Pavilion");

// As above, but notice that we still create bindings on the global, even
// when we've interposed a new environment via 'withBindings'.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.executeInGlobalWithBindings("eval('var x = d1;'); var y = d2; x;",
                                        { d1: "A Brief History of Love",
                                          d2: "Merriweather Post Pavilion" }).return,
         "A Brief History of Love");
assertEq(g.x, "A Brief History of Love");
assertEq(g.y, "Merriweather Post Pavilion");


// Strict mode code variants of the above:

// Strict mode still lets us create bindings on the global as this is
// equivalent to executing statements at the global level. But note that
// setting strict mode means that nested evals get their own call objects.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.executeInGlobal("\'use strict\';\n"
                            + "eval('var x = \"A Brief History of Love\"');\n"
                            + "var y = \"Merriweather Post Pavilion\";"
                            + "x;").return,
         "Swing Lo Magellan");
assertEq(g.x, "Swing Lo Magellan");
assertEq(g.y, "Merriweather Post Pavilion");

// Introducing a bindings object shouldn't change this behavior.
g.x = "Swing Lo Magellan";
g.y = "The Milk-Eyed Mender";
assertEq(gw.executeInGlobalWithBindings("'use strict'; eval('var x = d1;'); var y = d2; x;",
                                        { d1: "A Brief History of Love",
                                          d2: "Merriweather Post Pavilion" }).return,
         "Swing Lo Magellan");
assertEq(g.x, "Swing Lo Magellan");
assertEq(g.y, "Merriweather Post Pavilion");
