/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Create an object with a JavaScript setter. */
var firstSetterCount;
var o = { w:2, set x(v) { firstSetterCount++; } };

/*
 * Put the object in dictionary mode, so that JSObject::putProperty will 
 * mutate its shapes instead of creating new ones.
 */
delete o.w;

/* A stock watcher function. */
var watcherCount;
function watcher(id, oldval, newval) { watcherCount++; return newval; }

/*
 * Place a watchpoint on the property. The property's shape now has the
 * watchpoint setter, with the original setter saved in the watchpoint
 * structure.
 */
o.watch('x', watcher);

/*
 * Replace the setter with a new setter. The shape should get updated to
 * refer to the new setter, and then the watchpoint setter should be
 * re-established.
 */
var secondSetterCount;
Object.defineProperty(o, 'x', { set: function () { secondSetterCount++ } });

/*
 * Assign to the property. This should trigger the watchpoint and the new setter.
 */
watcherCount = firstSetterCount = secondSetterCount = 0;
o.x = 3;
assertEq(watcherCount, 1);
assertEq(firstSetterCount, 0);
assertEq(secondSetterCount, 1);

reportCompare(true, true);
