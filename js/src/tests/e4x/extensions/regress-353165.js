/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 353165;
var summary = 'Do not crash with xml_getMethod';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

crash1();
crash2();

END();

function crash1()
{
    try {
        var set = new XML('<a><b>text</b></a>').children('b');
        var counter = 0;
        Object.defineProperty(Object.prototype, "unrooter",
        { enumerable: true, configurable: true,
          get: function() {
            ++counter;
            if (counter == 5) {
                set[0] = new XML('<c/>');
                if (typeof gc == "function") {
                    gc();
                    var tmp = Math.sqrt(2), tmp2;
                    for (i = 0; i != 50000; ++i)
                        tmp2 = tmp / 2;
                }
            }
            return undefined;
          } });

        set.unrooter();
    }
    catch(ex) {
        print('1: ' + ex);
    }
    TEST(1, expect, actual);

}

function crash2() {
    try {
        var expected = "SOME RANDOM TEXT";

        var set = <a><b>{expected}</b></a>.children('b');
        var counter = 0;

        function unrooter_impl() {
                return String(this);
        }

        Object.defineProperty(Object.prototype, "unrooter",
        { enumerable: true, configurable: true,
          get: function() {
            ++counter;
            if (counter == 7)
            return unrooter_impl;
            if (counter == 5) {
                set[0] = new XML('<c/>');
                if (typeof gc == "function") {
                    gc();
                    var tmp = 1e500, tmp2;
                    for (i = 0; i != 50000; ++i)
                        tmp2 = tmp / 1.1;
                }
            }
            return undefined;
          } });

        set.unrooter();
    }
    catch(ex) {
        print('2: ' + ex);
    }
    TEST(2, expect, actual);
}
