// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 373072;
var summary = 'XML.prototype.namespace() does not check for xml list';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);

try
{
    expect = "TypeError: can't call namespace method on an XML list with 0 elements";
    XML.prototype.function::namespace.call(new XMLList());
}
catch(ex)
{
    actual = ex + '';
}
TEST(1, expect, actual);

END();
