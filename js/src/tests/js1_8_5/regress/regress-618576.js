/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Gary Kwong and Jason Orendorff
 */

var x = <x/>;
x.function::__proto__ = evalcx('');
for (a in x)  // don't assert
    ;

reportCompare(0, 0, 'ok');
