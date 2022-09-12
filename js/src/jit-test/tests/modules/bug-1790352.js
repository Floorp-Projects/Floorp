let a = registerModule('a', parseModule("import 'b';"));
let b = registerModule('b', parseModule("import 'c'; await 1; throw 'error';"));
let c = registerModule('c', parseModule("import 'b';"));

let status1;
import('a').then(() => { status1 = 'loaded' }).catch(e => { status1 = e; });
drainJobQueue();
assertEq(status1, 'error');

let status2;
import('c').then(() => { status2 = 'loaded' }).catch(e => { status2 = e; });
drainJobQueue();
assertEq(status2, 'error');
