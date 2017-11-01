/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 23 May 2001
 *
 * SUMMARY: Regression test for Bugzilla bug 82306
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=82306
 *
 * This test used to crash the JS engine. This was discovered
 * by Mike Epstein <epstein@tellme.com>
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 82306;
var summary = "Testing we don't crash on encodeURI()";
var URI = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  URI += '<?xml version="1.0"?>';
  URI += '<zcti application="xxxx_demo">';
  URI += '<pstn_data>';
  URI += '<ani>650-930-xxxx</ani>';
  URI += '<dnis>877-485-xxxx</dnis>';
  URI += '</pstn_data>';
  URI += '<keyvalue key="name" value="xxx"/>';
  URI += '<keyvalue key="phone" value="6509309000"/>';
  URI += '</zcti>';

  // Just testing that we don't crash on this
  encodeURI(URI);

  reportCompare('No Crash', 'No Crash', '');
}
