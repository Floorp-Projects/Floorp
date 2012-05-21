// |reftest| skip-if(!xulRuntime.shell) slow
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 350238;
var summary = 'Do not assert <x/>.@*++';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

if (typeof document != 'undefined' && 'addEventListener' in document)
{
    document.addEventListener('load',
                              (function () {
                                  var iframe = document.createElement('iframe');
                                  document.body.appendChild(iframe);
                                  iframe.contentDocument.location.href='javascript:<x/>.@*++;';
                              }), true);
}
else
{
    <x/>.@*++;
}

TEST(1, expect, actual);

END();
