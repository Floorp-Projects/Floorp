/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 290774;
var summary = 'activation object never delegates to Object.prototype';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var toStringResult;
var evalResult;
var watchResult;
var parseIntResult;

var eval = 'fooEval';
var watch = undefined;
var parseInt = 'fooParseInt';


function toString()
{
  return 'fooString';
}

function normal()
{
  toStringResult = toString;
  evalResult = eval;
  watchResult = watch;
  parseIntResult = parseInt;
}

function outerinnervar()
{
  toStringResult = toString;
  evalResult = eval;
  watchResult = watch;
  parseIntResult = parseInt;
  function inner()
  {
    // addition of any statement
    // which accesses a variable
    // from the outer scope causes the bug
    printStatus(toString);
  }
}

expect = true;

printStatus('normal');
printStatus('======');
normal();

printStatus('toStringResult ' + toStringResult);
printStatus('toString ' + toString);
actual = ((toStringResult + '') == (toString + ''));
reportCompare(expect, actual, inSection(1));

printStatus('evalResult ' + evalResult);
printStatus('eval ' + eval);
actual = ((evalResult + '') == (eval + ''));
reportCompare(expect, actual, inSection(2));

printStatus('watchResult ' + watchResult);
printStatus('watch ' + watch);
actual = ((watchResult + '') == (watch + ''));
reportCompare(expect, actual, inSection(3));

printStatus('parseIntResult ' + parseIntResult);
printStatus('parseInt ' + parseInt);
actual = ((parseIntResult + '') == (parseInt + ''));
reportCompare(expect, actual, inSection(4));

printStatus('outerinner');
printStatus('==========');

outerinnervar();

printStatus('toStringResult ' + toStringResult);
printStatus('toString ' + toString);
actual = ((toStringResult + '') == (toString + ''));
reportCompare(expect, actual, inSection(5));


printStatus('evalResult ' + evalResult);
printStatus('eval ' + eval);
actual = ((evalResult + '') == (eval + ''));
reportCompare(expect, actual, inSection(6));

printStatus('watchResult ' + watchResult);
printStatus('watch ' + watch);
actual = ((watchResult + '') == (watch + ''));
reportCompare(expect, actual, inSection(7));

printStatus('parseIntResult ' + parseIntResult);
printStatus('parseInt ' + parseInt);
actual = ((parseIntResult + '') == (parseInt + ''));
reportCompare(expect, actual, inSection(8));
