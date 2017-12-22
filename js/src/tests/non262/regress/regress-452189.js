/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Geoff Garen
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452189;
var summary = "Don't shadow a readonly or setter proto-property";
var expect = "PASS";
var actual = "FAIL";

function c() {
    this.x = 3;
}


new c;
Object.prototype.__defineSetter__('x', function(){ actual = expect; })
new c;

reportCompare(expect, actual, summary);
