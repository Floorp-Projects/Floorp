/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var s = "grape";
function f() { "use strict"; return this; }
var p = Proxy.createFunction(f, f);
String.prototype.p = p;
assertEq(s.p(), "grape");

reportCompare(true,true);
