// |reftest| skip-if(Android) silentfail
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Infinite recursion should throw catchable exception';
var BUGNUMBER = 394941;
var actual = '';
var expect = /InternalError: too much recursion/;

expectExitCode(0);
expectExitCode(5);

/*
 * use the reportMatch so that the test will pass on 1.8 
 * where the error message is "too much recursion" and on 1.9.0
 * where the error message is "script stack space quota is exhausted".
 */

printBugNumber(BUGNUMBER);
START(summary);

try 
{
    function f() { var z = <x><y/></x>; f(); }
    f();
} 
catch(ex) 
{
    actual = ex + '';
    print("Caught: " + ex);
}

reportMatch(expect, actual);

END();
