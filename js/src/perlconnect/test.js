/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*******************************************************************************
* PerlConnect test file. Some primitive testing in "silent mode" -- that is,
* you should be able to run
*           perlconnectshell test.js
* without any error messages. See README.html for more info.
*******************************************************************************/

// Init
assert(p = new Perl('Sys::Hostname', 'Time::gmtime'), "Perl initialization failed");
// Simple eval
assert(p.eval("'-' x 3") == '---', "Wrong value returned from eval");

assert(p.eval("undef()")==undefined, "Wrong value returned from eval");
// Arrays
assert(a=p.eval("(1, 2, 3);"), "eval failed, 1");
assert(a[1]==2, "Wrong value");
assert(a.length==3, "Wrong length");
// Hashes
assert(h=p.eval("{'one'=>1, 'two'=>2};"), "eval failed, 2");
assert(h["two"]==2, "Wrong value");
// Func. call
assert(p.eval("&hostname()") == p.call("hostname"), "Wrong value returned from eval or call");
// Complex call
assert(b=p.Time.gmtime.gmtime(), "call failed");
assert(b.length==9, "Wrong length")
// Variables
// Scalars
assert(p.eval("$a = 100; $b = 'abc';"), "eval failed, 3");
assert(p.$a ==100, "Wrong variable value, 1");
assert(p["$b"] == 'abc', "Wrong variable value, 2");

/* Auxilary function */
function assert(cond, msg)
{
    cond || print("Error: " + msg+"\n");
}  // assert
