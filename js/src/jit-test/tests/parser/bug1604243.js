/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


setDeferredParserAlloc(true);
var res = eval(`
const Y = f => (x => f(v => x(x)(v)))(x => f(v => x(x)(v)));
const f = fac => n => (n <= 1) ? 1 : n * fac(n - 1);
Y(f)(5)
`);
assertEq(res, 120);
