// |reftest| pref(javascript.options.xml.content,true) skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 324278;
var summary = 'GC without recursion';
var actual;
var expect;

printBugNumber(BUGNUMBER);
START(summary);

var N = 1000 * 1000;

print("N = " + N);

function prepare_list(N)
{
    var cursor = null;
    for (var i = 0; i != N; ++i) {
        var ns = new Namespace("protocol:address"); 
        ns.property = cursor;
        var xml = <xml/>;
        xml.addNamespace(ns);
        cursor = {xml: xml};
    }
    return cursor;
}

print("preparing...");

var list = prepare_list(N);

print("prepared for "+N);
gc();

var count = 0;
while (list && list.xml.inScopeNamespaces().length > 0 && list.xml.inScopeNamespaces()[1]) {
        list = list.xml.inScopeNamespaces()[1].property;
        ++count;
}

expect = N;
actual = count;

TEST(1, expect, actual);

gc();
 
END();
