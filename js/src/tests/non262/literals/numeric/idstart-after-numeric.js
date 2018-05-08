/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var BUGNUMBER = '523401';
var summary = 'numeric literal followed by an identifier';

var array = new Array();
assertThrowsInstanceOf(() => eval("array[0for]"), SyntaxError);
assertThrowsInstanceOf(() => eval("array[1yield]"), SyntaxError);
assertThrowsInstanceOf(() => eval("array[2in []]"), SyntaxError); // "2 in []" is valid.
reportCompare(array[2 in []], undefined);
reportCompare(2 in [], false);
assertThrowsInstanceOf(() => eval("array[3in]"), SyntaxError);
if (typeof reportCompare === "function")
    reportCompare(0, 0);
