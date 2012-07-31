// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'E4X - Do not crash on XML initializer <b{b}>';
var BUGNUMBER = 318922;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

var a, b, x;

printStatus('b=2;a=<a><b{b}>x</b{b}></a>');
b=2;
a=<a><b{b}>x</b{b}></a>;
TEST(1, expect, actual);

printStatus('b="c"; a=<a><b {b}="c">x</b></a>; ');
b="c";
a=<a><b {b}="c">x</b></a>;
TEST(2, expect, actual);

try
{
    a='';
    b='"';
    eval('a=<a><b c={b}x">x</b></a>');
}
catch(e)
{
    printStatus(e);
}
TEST(3, expect, actual);

try
{
    a='';
    b='"';
    eval('a=<a><b c="x{b}>x</b></a>');
}
catch(e)
{
    printStatus(e);
}
TEST(4, expect, actual);

try
{
    a='';
    b='x';
    eval('a=<a><b c={b}"x">x</b></a>');
}
catch(e)
{
    printStatus(e);
}
TEST(5, expect, actual);

try
{
    a='';
    b='x';
    eval('a=<a><b c="x"{b}>x</b></a>');
}
catch(e)
{
    printStatus(e);
}
TEST(6, expect, actual);

END();
