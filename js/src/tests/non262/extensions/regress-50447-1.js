/* -*- tab-width: 8; indent-tabs-mode: nil; js-indent-level: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/*
 * SUMMARY: New properties fileName, lineNumber have been added to Error objects
 * in SpiderMonkey. These are non-ECMA extensions and do not exist in Rhino.
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=50447
 *
 * 2005-04-05 Modified by bclary to support changes to error reporting
 *            which set default values for the error's fileName and
 *            lineNumber properties.
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 50447;
var summary = 'Test (non-ECMA) Error object properties fileName, lineNumber';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{

  printBugNumber(BUGNUMBER);
  printStatus (summary);

  testRealError();
  test1();
  test2();
  test3();
  test4();


}

// Normalize filenames so this test can work on Windows. This 
// function is also used on strings that contain filenames.
function normalize(filename)
{
    // Also convert double-backslash to single-slash to handle
    // escaped filenames in string literals.
    return filename.replace(/\\{1,2}/g, '/');
}

function testRealError()
{
  /* throw a real error, and see what it looks like */


  try
  {
    blabla;
  }
  catch (e)
  {
    if (e.fileName.search (/-50447-1\.js$/i) == -1)
      reportCompare('PASS', 'FAIL', "expected fileName to end with '-50447-1.js'");

    reportCompare(60, e.lineNumber,
		  "lineNumber property returned unexpected value.");
  }


}


function test1()
{
  /* generate an error with msg, file, and lineno properties */


  var e = new InternalError ("msg", "file", 2);
  reportCompare ("(new InternalError(\"msg\", \"file\", 2))",
		 e.toSource(),
		 "toSource() returned unexpected result.");
  reportCompare ("file", e.fileName,
		 "fileName property returned unexpected value.");
  reportCompare (2, e.lineNumber,
		 "lineNumber property returned unexpected value.");


}


function test2()
{
  /* generate an error with only msg property */


  /* note this test incorporates the path to the
     test file and assumes the path to the test case
     is a subdirectory of the directory containing jsDriver.pl
  */
  var expectedLine = 106;
  var expectedFileName = 'non262/extensions/regress-50447-1.js';
  var expectedSource = /\(new InternalError\("msg", "([^"]+)", ([0-9]+)\)\)/;

  var e = new InternalError ("msg");

  var actual = expectedSource.exec(e.toSource());
  reportCompare (normalize(actual[1]).endsWith(expectedFileName), true,
		 "toSource() returned unexpected result (filename).");
  reportCompare (actual[2], String(expectedLine),
		 "toSource() returned unexpected result (line).");
  reportCompare (normalize(e.fileName).endsWith(expectedFileName), true,
		 "fileName property returned unexpected value.");
  reportCompare (expectedLine, e.lineNumber,
		 "lineNumber property returned unexpected value.");


}


function test3()
{
  /* generate an error with only msg and lineNo properties */

  /* note this test incorporates the path to the
     test file and assumes the path to the test case
     is a subdirectory of the directory containing jsDriver.pl
  */



  var expectedLine = 10;
  var expectedFileName = 'non262/extensions/regress-50447-1.js';
  var expectedSource = /\(new InternalError\("msg", "([^"]+)", ([0-9]+)\)\)/;

  var e = new InternalError ("msg");
  e.lineNumber = expectedLine;

  var actual = expectedSource.exec(e.toSource());
  reportCompare (normalize(actual[1]).endsWith(expectedFileName), true,
		 "toSource() returned unexpected result (filename).");
  reportCompare (actual[2], String(expectedLine),
		 "toSource() returned unexpected result (line).");
  reportCompare (normalize(e.fileName).endsWith(expectedFileName), true,
		 "fileName property returned unexpected value.");
  reportCompare (expectedLine, e.lineNumber,
		 "lineNumber property returned unexpected value.");


}


function test4()
{
  /* generate an error with only msg and filename properties */


  var expectedLine = 161;

  var e = new InternalError ("msg", "file");
  reportCompare ("(new InternalError(\"msg\", \"file\", " + expectedLine + "))",
		 e.toSource(),
		 "toSource() returned unexpected result.");
  reportCompare ("file", e.fileName,
		 "fileName property returned unexpected value.");
  reportCompare (expectedLine, e.lineNumber,
		 "lineNumber property returned unexpected value.");


}
