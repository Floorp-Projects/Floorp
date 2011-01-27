var COUNT = RUNLOOP;
eval("'use strict'; for (let j = 0; j < COUNT; j++); try { x; throw new Error(); } catch (e) { if (!(e instanceof ReferenceError)) throw e; }");
assertEq(typeof j, "undefined");
