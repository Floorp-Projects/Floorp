/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
 * JavaScript test library shared functions file for running the tests
 * in the browser.  Overrides the shell's print function with document.write
 * and make everything HTML pretty.
 *
 * To run the tests in the browser, use the mkhtml.pl script to generate
 * html pages that include the shell.js, browser.js (this file), and the
 * test js file in script tags.
 *
 * The source of the page that is generated should look something like this:
 *      <script src="./../shell.js"></script>
 *      <script src="./../browser.js"></script>
 *      <script src="./mytest.js"></script>
 */
function writeLineToLog( string ) {
  document.write( string + "<br>\n");
}

var testcases = new Array();
var tc = testcases.length;
var bug = '';
var summary = '';
var description = '';
var expected = '';
var actual = '';
var msg = '';


function TestCase(n, d, e, a)
{
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = ( e == a );
  this.reason = '';
  this.bugnumber = typeof(bug) != 'undefined' ? bug : '';
  testcases[tc++] = this;
}

var gInReportCompare = false;

var _reportCompare = reportCompare;

reportCompare = function(expected, actual, description)
{
  gInReportCompare = true;

  var testcase = new TestCase(gTestName, description, expected, actual);
  testcase.passed = _reportCompare(expected, actual, description);

  gInReportCompare = false;
};

var _reportFailure = reportFailure;
reportFailure = function (msg, page, line)
{
  var testcase;

  if (gInReportCompare)
  {
    testcase = testcases[tc - 1];
    testcase.passed = false;
  }
  else 
  {
    if (typeof DESCRIPTION == 'undefined')
    {
      DESCRIPTION = 'Unknown';
    }
    if (typeof EXPECTED == 'undefined')
    {
      EXPECTED = 'Unknown';
    }
    testcase = new TestCase(gTestName, DESCRIPTION, EXPECTED, "error");
  }

  testcase.reason += msg;

  if (typeof(page) != 'undefined')
  {
    testcase.reason += ' Page: ' + page;
  }
  if (typeof(line) != 'undefined')
  {
    testcase.reason += ' Line: ' + line;
  }
  _reportFailure(msg);

};

function gc()
{
}

function quit()
{
}

window.onerror = reportFailure;
