/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346494;
var summary = 'various try...catch tests';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var pfx = "(function (x) {try {throw x}",
    cg1 = " catch (e if e === 42) {var v = 'catch guard 1 ' + e; actual += v + ','; print(v);}"
    cg2 = " catch (e if e === 43) {var v = 'catch guard 2 ' + e; actual += v + ','; print(v);}"
    cat = " catch (e) {var v = 'catch all ' + e; actual += v + ','; print(v);}"
    fin = " finally{var v = 'fin'; actual += v + ','; print(v)}",
    end = "})";

  var exphash = {
    pfx: "(function (y) { var result = ''; y = y + ',';",
    cg1: "result += (y === '42,') ? ('catch guard 1 ' + y):'';",
    cg2: "result += (y === '43,') ? ('catch guard 2 ' + y):'';",
    cat: "result += /catch guard/.test(result) ? '': ('catch all ' + y);",
    fin: "result += 'fin,';",
    end: "return result;})"
  };

  var src = [
    pfx + fin + end,
    pfx + cat + end,
    pfx + cat + fin + end,
    pfx + cg1 + end,
    pfx + cg1 + fin + end,
    pfx + cg1 + cat + end,
    pfx + cg1 + cat + fin + end,
    pfx + cg1 + cg2 + end,
    pfx + cg1 + cg2 + fin + end,
    pfx + cg1 + cg2 + cat + end,
    pfx + cg1 + cg2 + cat + fin + end,
    ];

  var expsrc = [
    exphash.pfx + exphash.fin + exphash.end,
    exphash.pfx + exphash.cat + exphash.end,
    exphash.pfx + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cg2 + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cg2 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cg2 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1 + exphash.cg2 + exphash.cat + exphash.fin + exphash.end,
    ];

  for (var i in src) {
    print("\n=== " + src[i]);
    var f = eval(src[i]);
    print(src[i]);
    var exp = eval(expsrc[i]);
    // dis(f);
    print('decompiling: ' + f);

    actual = '';
    try { expect = exp(42); f(42) } catch (e) { print('tried f(42), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(43); f(43) } catch (e) { print('tried f(43), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(44); f(44) } catch (e) { print('tried f(44), caught ' + e) }
    reportCompare(expect, actual, summary);
  }


  exitFunc ('test');
}
