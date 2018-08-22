// |reftest| skip-if(!this.hasOwnProperty("Intl"))
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1484943;
var summary = "Don't crash doing format/formatToParts on -NaN";

print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

assertEq("formatToParts" in Intl.NumberFormat(), true);

var nf = new Intl.NumberFormat("en-US");
var parts;

var values = [NaN, -NaN];

for (var v of values)
{
  assertEq(nf.format(v), "NaN");

  parts = nf.formatToParts(v);
  assertEq(parts.length, 1);
  assertEq(parts[0].type, "nan");
  assertEq(parts[0].value, "NaN");
}

//-----------------------------------------------------------------------------

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');

print("Tests complete");
