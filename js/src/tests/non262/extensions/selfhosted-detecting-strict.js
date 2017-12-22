// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 843004;
var summary =
  "Don't emit a strict warning for the undefined-property detection pattern in self-hosted code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

options("strict", "werror");

// Don't strict-warn (and throw, because of strict) when self-hosted code uses
// detecting-safe undefined-property accesses (|options.weekday !== undefined|
// and similar in ToDateTimeOptions, to be precise).
new Date().toLocaleString("en-US", {});

// If we get here, the test passed.

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
