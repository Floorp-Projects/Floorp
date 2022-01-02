/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 587366;
var summary = "String.prototype.replace with non-regexp searchValue";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

/* 
 * Check that regexp statics are preserved across the whole test.
 * If the engine is trying to cheat by turning stuff into regexps,
 * we should catch it!
 */
/(a|(b)|c)+/.exec('abcabc');
var before = {
    "source" : RegExp.source,
    "$`": RegExp.leftContext,
    "$'": RegExp.rightContext,
    "$&": RegExp.lastMatch,
    "$1": RegExp.$1,
    "$2": RegExp.$2
};

var text = 'I once was lost but now am found.';
var searchValue = 'found';
var replaceValue;

/* Lambda substitution. */
replaceValue = function(matchStr, matchStart, textStr) {
    assertEq(matchStr, searchValue);
    assertEq(matchStart, 27);
    assertEq(textStr, text);
    return 'not watching that show anymore';
}
var result = text.replace(searchValue, replaceValue);
assertEq(result, 'I once was lost but now am not watching that show anymore.');

/* Dollar substitution. */
replaceValue = "...wait, where was I again? And where is all my $$$$$$? Oh right, $`$&$'" +
               " But with no $$$$$$"; /* Note the dot is not replaced and trails the end. */
result = text.replace(searchValue, replaceValue);
assertEq(result, 'I once was lost but now am ...wait, where was I again?' +
                 ' And where is all my $$$? Oh right, I once was lost but now am found.' +
                 ' But with no $$$.');

/* Missing capture group dollar substitution. */
replaceValue = "$1$&$2$'$3";
result = text.replace(searchValue, replaceValue);
assertEq(result, 'I once was lost but now am $1found$2.$3.');

/* Check RegExp statics haven't been mutated. */
for (var ident in before)
    assertEq(RegExp[ident], before[ident]);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
