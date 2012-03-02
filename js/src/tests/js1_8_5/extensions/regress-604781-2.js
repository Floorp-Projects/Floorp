// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var log;
function watcher(id, old, newval) { log += 'watcher'; return newval; }
var o = { set x(v) { log += 'setter'; } };
o.watch('x', watcher);
Object.defineProperty(o, 'x', {value: 3, writable: true});
log = '';
o.x = 3;
assertEq(log, 'watcher');

reportCompare(0, 0, 'ok');
