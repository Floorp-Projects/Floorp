// Copyright 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: Function.prototype.toString on an async generator method
features: [async-iteration]
---*/

let f = { /* before */async /* a */ * /* b */ f /* c */ ( /* d */ ) /* e */ { /* f */ }/* after */ }.f;
let g = { /* before */async /* a */ * /* b */ [ /* c */ "g" /* d */ ] /* e */ ( /* f */ ) /* g */ { /* h */ }/* after */ }.g;

assert.sameValue(f.toString(), "async /* a */ * /* b */ f /* c */ ( /* d */ ) /* e */ { /* f */ }");
assert.sameValue(g.toString(), "async /* a */ * /* b */ [ /* c */ \"g\" /* d */ ] /* e */ ( /* f */ ) /* g */ { /* h */ }");

reportCompare(0, 0);
