// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate()
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1046964;
var summary =
  "Misplaced directives (e.g. 'use strict') should trigger warnings if they " +
  "contradict the actually-used semantics";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

options("strict");
options("werror");

function evaluateNoRval(code)
{
  evaluate(code, { isRunOnce: true, noScriptRval: true });
}

function expectSyntaxError(code)
{
  try
  {
    evaluateNoRval(code);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof SyntaxError, true,
             "should have thrown a SyntaxError, instead got:\n" +
             "    " + e + "\n" +
             "when evaluating:\n" +
             "    " + code);
  }
}

expectSyntaxError("function f1() {} 'use strict'; function f2() {}");
expectSyntaxError("function f3() { var x; 'use strict'; }");

if (isAsmJSCompilationAvailable())
  expectSyntaxError("function f4() {} 'use asm'; function f5() {}");
expectSyntaxError("function f6() { var x; 'use strict'; }");
if (isAsmJSCompilationAvailable())
  expectSyntaxError("'use asm'; function f7() {}");

// No errors expected -- useless non-directives, but not contrary to used
// semantics.
evaluateNoRval("'use strict'; function f8() {} 'use strict'; function f9() {}");
evaluateNoRval("'use strict'; function f10() { var z; 'use strict' }");

if (isAsmJSCompilationAvailable()) {
  try {
    evaluateNoRval("function f11() { 'use asm'; return {}; }");
  } catch(e) {
    if (!e.toString().includes("Successfully compiled asm.js code"))
      throw e;
  }
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
