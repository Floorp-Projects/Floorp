/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
