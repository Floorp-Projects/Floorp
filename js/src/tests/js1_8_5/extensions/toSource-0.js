/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
 
assertEq(eval(uneval('\x001')), '\x001');

f = eval('(' + (function () { return '\x001'; }).toString() + ')');
assertEq(f(), '\x001');

assertEq(eval('\x001'.toSource()) == '\x001', true);

if (typeof reportCompare === 'function')
  reportCompare(true, true);
