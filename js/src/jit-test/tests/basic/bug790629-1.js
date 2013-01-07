// 'this' in generator-expression in strict-mode toplevel
// is the same as global 'this'.

"use strict";

var it1 = (this for (x of [0]));
assertEq(it1.next(), this);

var it2 = (this for (x of (this for (y of (this for (z of [0]))))));
assertEq(it2.next(), this);
