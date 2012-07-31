// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "13.3.5.2 - root QName.uri";
var BUGNUMBER = 313952;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
START(summary);
printStatus("This test requires TOO_MUCH_GC");

var str = " foo:bar".substring(1);
expect = new QName("  foo:bar".substring(2), "a").uri;

var likeString = {
        toString: function() {
                var tmp = str;
                str = null;
                return tmp;
        }
};

actual = new QName(likeString, "a").uri;

printStatus(actual.length);

printStatus(expect === actual);

TEST(1, expect, actual);

END();
