/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* A stock watcher function. */
var watcherCount;
function watcher(id, old, newval) {
    watcherCount++;
    return newval; 
}

/* Create an object with a value property. */
var o = { w:2, x:3 };

/*
 * Place a watchpoint on the value property. The watchpoint structure holds
 * the original JavaScript setter, and a pointer to the shape.
 */
o.watch('x', watcher);

/*
 * Put the object in dictionary mode, so that JSObject::putProperty will 
 * mutate its shapes instead of creating new ones.
 */
delete o.w;

/*
 * Replace the value property with a setter.
 */
var setterCount;
o.__defineSetter__('x', function() { setterCount++; });

/*
 * Trigger the watchpoint. The watchpoint handler should run, and then the
 * setter should run.
 */
watcherCount = setterCount = 0;
o.x = 4;
assertEq(watcherCount, 1);
assertEq(setterCount, 1);

reportCompare(true, true);
