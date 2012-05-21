/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "GC hazard during namespace scanning";
var BUGNUMBER = 324117;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function prepare(N)
{
    var xml = <xml/>;
    var ns1 = new Namespace("text1"); 
    var ns2 = new Namespace("text2"); 
    xml.addNamespace(ns1);
    xml.addNamespace(ns2);

    // Prepare list to trigger DeutschSchorrWaite call during GC
    cursor = xml;
    for (var i = 0; i != N; ++i) {
        if (i % 2 == 0)
            cursor = [ {a: 1}, cursor ];
        else
            cursor = [ cursor, {a: 1} ];
    }
    return cursor;
}

function check(list, N)
{
    // Extract xml to verify
    for (var i = N; i != 0; --i) {
        list = list[i % 2];
    }
    var xml = list;
    if (typeof xml != "xml")
        return false;
    var array = xml.inScopeNamespaces();
    if (array.length !== 3)
        return false;
    if (array[0].uri !== "")
        return false;
    if (array[1].uri !== "text1")
        return false;
    if (array[2].uri !== "text2")
        return false;

    return true;
}

var N = 64000;
var list = prepare(N);
gc();
var ok = check(list, N);
printStatus(ok);

TEST(1, expect, actual);

END();
