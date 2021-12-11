/* 
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: haytjes <hv1989@gmail.com>
 */

/* Check the undefined pattern is equivalent to empty string. */

assertEq(RegExp(undefined).source, '(?:)');
assertEq(RegExp(undefined).global, false);
assertEq("test".replace(RegExp(undefined), "*"), '*test');
assertEq(new RegExp(undefined).source, '(?:)');
assertEq(new RegExp(undefined).global, false);
assertEq('test'.replace(new RegExp(undefined), "*"), '*test');

/* Global flags. */

assertEq(new RegExp(undefined, "g").global, true);
assertEq("test".replace(new RegExp(undefined, "g"), "*"), "*t*e*s*t*");
assertEq(RegExp(undefined, "g").global, true);
assertEq("test".replace(RegExp(undefined, "g"), "*"), "*t*e*s*t*");

/* Undefined flags. */

var re = new RegExp(undefined, undefined);
assertEq(re.multiline, false);
assertEq(re.global, false);
assertEq(re.ignoreCase, false);

var re = new RegExp("test", undefined);
assertEq(re.multiline, false);
assertEq(re.global, false);
assertEq(re.ignoreCase, false);

/* Flags argument that requires toString. */

function Flags() {};

Flags.prototype.toString = function dogToString() { return ""; }

var re = new RegExp(undefined, new Flags());
assertEq(re.multiline, false);
assertEq(re.global, false);
assertEq(re.ignoreCase, false);

Flags.prototype.toString = function dogToString() { return "gim"; }

var re = new RegExp(undefined, new Flags());
assertEq(re.multiline, true);
assertEq(re.global, true);
assertEq(re.ignoreCase, true);
