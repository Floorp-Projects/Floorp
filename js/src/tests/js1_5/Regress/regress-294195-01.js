/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 294195;
var summary = 'Do not crash during String replace when accessing methods on backreferences';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var s = 'some text sample';

// this first version didn't crash
var result = s.replace(new RegExp('(^|\\s)(text)'), (new String('$1')));
result = result.substr(0, 1);
reportCompare(expect, actual, inSection(1) + ' ' + summary);

// the original version however did crash.
result = s.replace(new RegExp('(^|\\s)(text)'),
                   (new String('$1')).substr(0, 1));
reportCompare(expect, actual, inSection(2) + ' ' + summary);
