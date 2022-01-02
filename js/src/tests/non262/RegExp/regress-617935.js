// |reftest| skip-if(!xulRuntime.shell&&(Android||xulRuntime.OS=="WINNT")) silentfail
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 *
 * Author: Christian Holler <decoder@own-hero.net>
 */

expectExitCode(0);
expectExitCode(5);

/* Length of 32 */
var foo = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

/* Make len(foo) 32768 */
for (i = 0; i < 10; ++i) {
    foo += foo;
}

/* Add one "a" to cause overflow later */
foo += "a";

var bar = "bbbbbbbbbbbbbbbb";

/* Make len(bar) 8192 */
for (i = 0; i < 9; ++i) {
    bar += bar;
}

/* 
 * Resulting string should be 
 * len(foo) * len(bar) = (2**10 * 32 + 1) * 8192 = 268443648
 * which will be larger than the max string length (2**28, or 268435456).
 */
try {
    foo.replace(/[a]/g, bar);
} catch (e) {
    reportCompare(e instanceof InternalError, true, "Internal error due to overallocation is ok.");
}
reportCompare(true, true, "No crash occurred.");

print("Tests complete");
