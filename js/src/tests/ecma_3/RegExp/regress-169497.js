/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    31 August 2002
 * SUMMARY: RegExp conformance test
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=169497
 *
 */
//-----------------------------------------------------------------------------
var i = 0;
var BUGNUMBER = 169497;
var summary = 'RegExp conformance test';
var status = '';
var statusmessages = new Array();
var pattern = '';
var patterns = new Array();
var sBody = '';
var sHTML = '';
var string = '';
var strings = new Array();
var actualmatch = '';
var actualmatches = new Array();
var expectedmatch = '';
var expectedmatches = new Array();

sBody += '<body onXXX="alert(event.type);">\n';
sBody += '<p>Kibology for all<\/p>\n';
sBody += '<p>All for Kibology<\/p>\n';
sBody += '<\/body>';

sHTML += '<html>\n';
sHTML += sBody;
sHTML += '\n<\/html>';

status = inSection(1);
string = sHTML;
pattern = /<body.*>((.*\n?)*?)<\/body>/i;
actualmatch = string.match(pattern);
expectedmatch = Array(sBody, '\n<p>Kibology for all</p>\n<p>All for Kibology</p>\n', '<p>All for Kibology</p>\n');
addThis();



//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function addThis()
{
  statusmessages[i] = status;
  patterns[i] = pattern;
  strings[i] = string;
  actualmatches[i] = actualmatch;
  expectedmatches[i] = expectedmatch;
  i++;
}


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  testRegExp(statusmessages, patterns, strings, actualmatches, expectedmatches);
}
