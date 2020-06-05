// |reftest| skip-if(!xulRuntime.shell) -- needs Debugger
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1596706;
var summary =
  "Properly apply a source map directive that's only tokenized by a syntax " +
  "parser (because the directive comment appears immediately after an arrow " +
  "function with expression body)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

var script;
dbg.onDebuggerStatement = function (frame)
{
  script = frame.script;
};

function checkSourceMapFor(arrowFunc, sourceMap)
{
  g.evaluate(`${arrowFunc}
//# sourceMappingURL=${sourceMap}
debugger;`);

  assertEq(script instanceof Debugger.Script, true,
           "expected a Debugger.Script for " + arrowFunc);

  var source = script.source;
  assertEq(source instanceof Debugger.Source, true,
           "expected a Debugger.Source for " + arrowFunc);

  assertEq(source.sourceMapURL, sourceMap,
           "unexpected source map for " + arrowFunc);
}

// block followed by semicolon
checkSourceMapFor("x=>{};", "http://example.com/foo.js");

// block not followed by semicolon
checkSourceMapFor("x=>{}", "http://example.com/bar.js");

// expr followed by semicolon
checkSourceMapFor("x=>y;", "http://example.com/baz.js");

// expr not followed by semicolon
checkSourceMapFor("x=>y", "http://example.com/bar.js");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
