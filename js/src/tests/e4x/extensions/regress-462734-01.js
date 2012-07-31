// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not assert: pobj_ == obj2';
var BUGNUMBER = 462734;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

var save__proto__ = this.__proto__;

try
{
    for (x in function(){}) (<x><y/></x>);
    this.__defineGetter__("x", Function);
    __proto__ = x;
    prototype += <x><y/></x>;
}
catch(ex)
{
    print(ex + '');
}

__proto__ = save__proto__;

TEST(1, expect, actual);

END();
