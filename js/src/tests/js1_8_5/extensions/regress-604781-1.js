// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var watcherCount, setterCount;
function watcher(id, oldval, newval) { watcherCount++; return newval; }
function setter(newval) { setterCount++; }

var p = { set x(v) { setter(v); } };
p.watch('x', watcher);

watcherCount = setterCount = 0;
p.x = 2;
assertEq(setterCount, 1);
assertEq(watcherCount, 1);

var o = Object.defineProperty({}, 'x', { set:setter, enumerable:true, configurable:true });
o.watch('x', watcher);

watcherCount = setterCount = 0;
o.x = 2;
assertEq(setterCount, 1);
assertEq(watcherCount, 1);

reportCompare(0, 0, 'ok');
