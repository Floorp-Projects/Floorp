/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Create a prototype object with a setter property. */
var protoSetterCount;
var proto = ({ set x(v) { protoSetterCount++; } });

/* Put a watchpoint on that setter. */
var protoWatchCount;
proto.watch('x', function() { protoWatchCount++; });

/* Make an object with the above as its prototype. */
function C() { }
C.prototype = proto;
var o = new C();

/*
 * Set a watchpoint on the property in the inheriting object. We have
 * defined this to mean "duplicate the property, setter and all, in the
 * inheriting object." I don't think debugging observation mechanisms
 * should mutate the program being run, but that's what we've got.
 */
var oWatchCount;
o.watch('x', function() { oWatchCount++; });

/*
 * Assign to the property. This should trip the watchpoint on the inheriting object and
 * the setter.
 */
protoSetterCount = protoWatchCount = oWatchCount = 0;
o.x = 1;
assertEq(protoWatchCount, 0);
assertEq(oWatchCount, 1);
assertEq(protoSetterCount, 1);

reportCompare(true, true);
