// |reftest| pref(javascript.options.xml.content,true) skip-if(Android)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not assert: cursor == (uint8_t *)copy->messageArgs[0] + argsCopySize';
var BUGNUMBER = 473709;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

function f() { eval("(function() { switch(x, x) { default: for(x2; <x><y/></x>;) (function(){}) <x><y/></x>;break; case (+<><x><y/></x></>): break;   }; })()"); }

if (typeof gczeal == 'function')
{
    gczeal(2);
}

try
{
    f();
}
catch(ex)
{
}

if (typeof gczeal == 'function')
{
    gczeal(0);
}

TEST(1, expect, actual);

END();
