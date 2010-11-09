/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* A stock watcher function. */
var watcherCount;
function watcher(id, oldval, newval) { watcherCount++; return newval; }

/* Create an object with a JavaScript setter. */
var setterCount;
var o = { w:2, set x(v) { setterCount++; } };

/*
 * Put the object in dictionary mode, so that JSObject::putProperty will 
 * mutate its shapes instead of creating new ones.
 */
delete o.w;

/*
 * Place a watchpoint on the property. The watchpoint structure holds the
 * original JavaScript setter, and a pointer to the shape.
 */
o.watch('x', watcher);

/*
 * Replace the accessor property with a value property. The shape's setter
 * should become a non-JS setter, js_watch_set, and the watchpoint
 * structure's saved setter should be updated (in this case, cleared).
 */
Object.defineProperty(o, 'x', { value:3,
                                writable:true,
                                enumerable:true,
                                configurable:true });

/*
 * Assign to the property. This should trigger js_watch_set, which should
 * call the handler, and then see that there is no JS-level setter to pass
 * control on to, and return.
 */
watcherCount = setterCount = 0;
o.x = 3;
assertEq(watcherCount, 1);
assertEq(setterCount, 0);

reportCompare(true, true);
