/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var o = { f: function() { return o.g(); }, g: function() { return arguments.callee.caller; } };
var c = o.f();
var i = 'f';
var d = o[i]();

reportCompare(true, c === o.f && d === o.f(), "");
