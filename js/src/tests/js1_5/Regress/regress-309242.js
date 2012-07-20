// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 309242;
var summary = 'E4X should be on by default while preserving comment hack';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

/* 
    E4X should be available regardless of script type
    <!-- and --> should begin comment to end of line
    unless type=text/javascript;e4x=1
*/

expect = true;
actual = true;
// the next line will be ignored when e4x is not requested
<!-- comment -->; actual = false;

reportCompare(expect, actual, summary + ': &lt;!-- is comment to end of line');

expect = true;
actual = false;
// the next line will be ignored when e4x is not requested
<!--
 actual = true;
// -->

reportCompare(expect, actual, summary + ': comment hack works inside script');

// E4X is available always

var x = <foo/>;

expect = 'element';
actual = x.nodeKind();

reportCompare(expect, actual, summary + ': E4X is available');
