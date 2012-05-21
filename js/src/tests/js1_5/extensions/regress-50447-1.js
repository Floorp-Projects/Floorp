/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  testRealError();
  test1();
  test2();
  test3();
  test4();

  exitFunc('test');
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
  enterFunc ("testRealError");

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

  exitFunc ("testRealError");
}


function test1()
{
  /* generate an error with msg, file, and lineno properties */
  enterFunc ("test1");

  var e = new InternalError ("msg", "file", 2);
  reportCompare ("(new InternalError(\"msg\", \"file\", 2))",
		 e.toSource(),
		 "toSource() returned unexpected result.");
  reportCompare ("file", e.fileName,
		 "fileName property returned unexpected value.");
  reportCompare (2, e.lineNumber,
		 "lineNumber property returned unexpected value.");

  exitFunc ("test1");
}


function test2()
{
  /* generate an error with only msg property */
  enterFunc ("test2");

  /* note this test incorporates the path to the
     test file and assumes the path to the test case
     is a subdirectory of the directory containing jsDriver.pl
  */
  var expectedLine = 114;
  var expectedFileName = 'js1_5/extensions/regress-50447-1.js';
  if (typeof document == "undefined")
  {
    expectedFileName = './' + expectedFileName;
  }
  else
  {
    expectedFileName = document.location.href.
      replace(/[^\/]*(\?.*)$/, '') +
      expectedFileName;
  }
  var e = new InternalError ("msg");
  reportCompare ("(new InternalError(\"msg\", \"" +
		 expectedFileName + "\", " + expectedLine + "))",
		 normalize(e.toSource()),
		 "toSource() returned unexpected result.");
  reportCompare (expectedFileName, normalize(e.fileName),
		 "fileName property returned unexpected value.");
  reportCompare (expectedLine, e.lineNumber,
		 "lineNumber property returned unexpected value.");

  exitFunc ("test2");
}


function test3()
{
  /* generate an error with only msg and lineNo properties */

  /* note this test incorporates the path to the
     test file and assumes the path to the test case
     is a subdirectory of the directory containing jsDriver.pl
  */

  enterFunc ("test3");

  var expectedFileName = 'js1_5/extensions/regress-50447-1.js';
  if (typeof document == "undefined")
  {
    expectedFileName = './' + expectedFileName;
  }
  else
  {
    expectedFileName = document.location.href.
      replace(/[^\/]*(\?.*)$/, '') +
      expectedFileName;
  }

  var e = new InternalError ("msg");
  e.lineNumber = 10;
  reportCompare ("(new InternalError(\"msg\", \"" +
		 expectedFileName + "\", 10))",
		 normalize(e.toSource()),
		 "toSource() returned unexpected result.");
  reportCompare (expectedFileName, normalize(e.fileName),
		 "fileName property returned unexpected value.");
  reportCompare (10, e.lineNumber,
		 "lineNumber property returned unexpected value.");

  exitFunc ("test3");
}


function test4()
{
  /* generate an error with only msg and filename properties */
  enterFunc ("test4");

  var expectedLine = 173;

  var e = new InternalError ("msg", "file");
  reportCompare ("(new InternalError(\"msg\", \"file\", " + expectedLine + "))",
		 e.toSource(),
		 "toSource() returned unexpected result.");
  reportCompare ("file", e.fileName,
		 "fileName property returned unexpected value.");
  reportCompare (expectedLine, e.lineNumber,
		 "lineNumber property returned unexpected value.");

  exitFunc ("test4");
}
