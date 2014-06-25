/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    13 Sep 2002
 * SUMMARY: Testing F.toString()
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=168347
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 168347;
var summary = "Testing F.toString()";
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var FtoString = '';
var sFunc = '';

sFunc += 'function F()';
sFunc += '{';
sFunc += '  var f = arguments.callee;';
sFunc += '  f.i = 0;';
sFunc += '';
sFunc += '  try';
sFunc += '  {';
sFunc += '    f.i = f.i + 1;';
sFunc += '    print("i = i+1 succeeded \ti = " + f.i);';
sFunc += '  }';
sFunc += '  catch(e)';
sFunc += '  {';
sFunc += '    print("i = i+1 failed with " + e + "\ti = " + f.i);';
sFunc += '  }';
sFunc += '';
sFunc += '  try';
sFunc += '  {';
sFunc += '    ++f.i;';
sFunc += '    print("++i succeeded \t\ti = " + f.i);';
sFunc += '  }';
sFunc += '  catch(e)';
sFunc += '  {';
sFunc += '    print("++i failed with " + e + "\ti = " + f.i);';
sFunc += '  }';
sFunc += '';
sFunc += '  try';
sFunc += '  {';
sFunc += '    f.i++;';
sFunc += '    print("i++ succeeded \t\ti = " + f.i);';
sFunc += '  }';
sFunc += '  catch(e)';
sFunc += '  {';
sFunc += '    print("i++ failed with " + e + "\ti = " + f.i);';
sFunc += '  }';
sFunc += '';
sFunc += '  try';
sFunc += '  {';
sFunc += '    --f.i;';
sFunc += '    print("--i succeeded \t\ti = " + f.i);';
sFunc += '  }';
sFunc += '  catch(e)';
sFunc += '  {';
sFunc += '    print("--i failed with " + e + "\ti = " + f.i);';
sFunc += '  }';
sFunc += '';
sFunc += '  try';
sFunc += '  {';
sFunc += '    f.i--;';
sFunc += '    print("i-- succeeded \t\ti = " + f.i);';
sFunc += '  }';
sFunc += '  catch(e)';
sFunc += '  {';
sFunc += '    print("i-- failed with " + e + "\ti = " + f.i);';
sFunc += '  }';
sFunc += '}';


/*
 * Use sFunc to define the function F. The test
 * then rests on comparing F.toString() to sFunc.
 */
eval(sFunc);


/*
 * There are trivial whitespace differences between F.toString()
 * and sFunc. So strip out whitespace before comparing them -
 */
sFunc = stripWhite(sFunc);
FtoString = stripWhite(F.toString());


/*
 * Break comparison into sections to make any failures
 * easier for the developer to track down -
 */
status = inSection(1);
actual = FtoString.substring(0,100);
expect = sFunc.substring(0,100);
addThis();

status = inSection(2);
actual = FtoString.substring(100,200);
expect = sFunc.substring(100,200);
addThis();

status = inSection(3);
actual = FtoString.substring(200,300);
expect = sFunc.substring(200,300);
addThis();

status = inSection(4);
actual = FtoString.substring(300,400);
expect = sFunc.substring(300,400);
addThis();

status = inSection(5);
actual = FtoString.substring(400,500);
expect = sFunc.substring(400,500);
addThis();

status = inSection(6);
actual = FtoString.substring(500,600);
expect = sFunc.substring(500,600);
addThis();

status = inSection(7);
actual = FtoString.substring(600,700);
expect = sFunc.substring(600,700);
addThis();

status = inSection(8);
actual = FtoString.substring(700,800);
expect = sFunc.substring(700,800);
addThis();

status = inSection(9);
actual = FtoString.substring(800,900);
expect = sFunc.substring(800,900);
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}

/*
 * Remove any whitespace characters; also
 * any escaped tabs or escaped newlines.
 */
function stripWhite(str)
{
  var re = /\s|\\t|\\n/g;
  return str.replace(re, '');
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
