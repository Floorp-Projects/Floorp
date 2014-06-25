/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350312;
var summary = 'Accessing wrong stack slot with nested catch/finally';
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

  var pfx  = "(function (x) {try {if (x > 41) throw x}",
    cg1a = " catch (e if e === 42) {var v = 'catch guard 1 ' + e; actual += v + ',';print(v);}"
    cg1b = " catch (e if e === 42) {var v = 'catch guard 1 + throw ' + e; actual += v + ',';print(v); throw e;}"
    cg2  = " catch (e if e === 43) {var v = 'catch guard 2 ' + e; actual += v + ',';print(v)}"
    cat  = " catch (e) {var v = 'catch all ' + e; print(v); if (e == 44) throw e}"
    fin  = " finally{var v = 'fin'; actual += v + ',';print(v)}",
    end  = "})";

  var exphash  = {
    pfx: "(function (y) { var result = ''; y = y + ',';",
    cg1a: " result += (y === '42,') ? ('catch guard 1 ' + y):'';",
    cg1b: " result += (y === '42,') ? ('catch guard 1 + throw ' + y):'';",
    cg2:  " result += (y === '43,') ? ('catch guard 2 ' + y):'';",
    cat:  " result += (y > 41) ? ('catch all ' + y):'';",
    fin:  " result += 'fin,';",
    end:  "return result;})"
  };

  var src = [
    pfx + fin + end,
    pfx + cat + end,
    pfx + cat + fin + end,
    pfx + cg1a + end,
    pfx + cg1a + fin + end,
    pfx + cg1a + cat + end,
    pfx + cg1a + cat + fin + end,
    pfx + cg1a + cg2 + end,
    pfx + cg1a + cg2 + fin + end,
    pfx + cg1a + cg2 + cat + end,
    pfx + cg1a + cg2 + cat + fin + end,
    pfx + cg1b + end,
    pfx + cg1b + fin + end,
    pfx + cg1b + cat + end,
    pfx + cg1b + cat + fin + end,
    pfx + cg1b + cg2 + end,
    pfx + cg1b + cg2 + fin + end,
    pfx + cg1b + cg2 + cat + end,
    pfx + cg1b + cg2 + cat + fin + end,
    ];

  var expsrc = [
    exphash.pfx + exphash.fin + exphash.end,
    exphash.pfx + exphash.cat + exphash.end,
    exphash.pfx + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1a + exphash.cg2 + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cat + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.fin + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.cat + exphash.end,
    exphash.pfx + exphash.cg1b + exphash.cg2 + exphash.cat + exphash.fin + exphash.end,
    ];

  for (var i in src) {
    print("\n=== " + i + ": " + src[i]);
    var f = eval(src[i]);
    var exp = eval(expsrc[i]);
    // dis(f);
    print('decompiling: ' + f);
    //print('decompiling exp: ' + exp);

    actual = '';
    try { expect = exp(41); f(41) } catch (e) { print('tried f(41), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(42); f(42) } catch (e) { print('tried f(42), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(43); f(43) } catch (e) { print('tried f(43), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(44); f(44) } catch (e) { print('tried f(44), caught ' + e) }
    reportCompare(expect, actual, summary);

    actual = '';
    try { expect = exp(45); f(45) } catch (e) { print('tried f(44), caught ' + e) }
    reportCompare(expect, actual, summary);

  }

  exitFunc ('test');
}
