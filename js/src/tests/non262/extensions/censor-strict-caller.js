// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/


function nonstrict() { return nonstrict.caller; }
function strict() { "use strict"; return nonstrict(); }

assertEq(strict(), null);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
