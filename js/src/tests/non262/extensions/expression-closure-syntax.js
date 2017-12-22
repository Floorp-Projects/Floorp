// |reftest| skip-if(!xulRuntime.shell) -- needs getBuildConfiguration
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1416337;
var summary =
  "Expression closure syntax is only permitted for functions that constitute " +
  "entire AssignmentExpressions, not PrimaryExpressions that are themselves " +
  "components of larger binary expressions";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

if (getBuildConfiguration().release_or_beta)
{
  function assertThrowsSyntaxError(code)
  {
    function testOne(replacement)
    {
      var x, rv;
      try
      {
        rv = eval(code.replace("@@@", replacement));
      }
      catch (e)
      {
        assertEq(e instanceof SyntaxError, true,
                 "should have thrown a SyntaxError, instead got: " + e);
        return;
      }

      assertEq(true, false, "should have thrown, instead returned " + rv);
    }

    testOne("function");
    testOne("async function");
  }

  assertThrowsSyntaxError("x = ++@@@() 1");
  assertThrowsSyntaxError("x = delete @@@() 1");
  assertThrowsSyntaxError("x = new @@@() 1");
  assertThrowsSyntaxError("x = void @@@() 1");
  assertThrowsSyntaxError("x = +@@@() 1");
  assertThrowsSyntaxError("x = 1 + @@@() 1");
  assertThrowsSyntaxError("x = null != @@@() 1");
  assertThrowsSyntaxError("x = null != @@@() 0 ? 1 : a => {}");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {} !== null");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {}.toString");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {}['toString']");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {}``");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {}()");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {}++");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {} || 0");
  assertThrowsSyntaxError("x = 0 || @@@() 0 ? 1 : a => {}");
  assertThrowsSyntaxError("x = @@@() 0 ? 1 : a => {} && true");
  assertThrowsSyntaxError("x = true && @@@() 0 ? 1 : a => {}");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
