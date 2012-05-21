/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351070;
var summary = 'decompilation of let declaration should not change scope';
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
 
  try
  {
    var pfx  = "(function f() { var n = 2, a = 2; ",
      decl = " let a = 3;",
      end  = " return a; })";

    var table = [
      ["if (!!true)",       ""],
      ["if (!!true)",       " else foopy();"],
      ["if (!true); else",  ""],
      ["do ",               " while (false);"],
      ["while (--n)",       ""],
      ["for (--n;n;--n)",   ""],
      ["for (a in this)",   ""],
      ["with (this)",       ""],
      ];

    expect = 3;

    for (i = 0; i < table.length; i++) {
      var src = pfx + table[i][0] + decl + table[i][1] + end;
      print('src: ' + src);
      var fun = eval(src);
      var testval = fun();
      reportCompare(expect, testval, summary + ': ' + src);
      if (testval != expect) {
        break;
      }
      print('uneval: ' + uneval(fun));
      var declsrc = '(' +
        src.slice(1, -1).replace('function f', 'function f' + i) + ')';
      print('declsrc: ' + declsrc);
      this['f' + i] = eval(declsrc);
      print('f' + i + ': ' + this['f' + i]);
    }
  }
  catch(ex)
  {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=408957
    summary = 'let declaration must be direct child of block or top-level implicit block';
    expect = 'SyntaxError';
    actual = ex.name;
    reportCompare(expect, actual, summary);
  }

  exitFunc ('test');
}
