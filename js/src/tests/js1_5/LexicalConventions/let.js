/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var actual = 'No error';
try {
    eval("var let = true");
} catch (exc) {
    actual = exc + '';
}

reportCompare('SyntaxError: let is a reserved identifier', actual, 'ok'); 

