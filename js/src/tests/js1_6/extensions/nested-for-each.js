/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'for-each-in should not affect for-in';
var BUGNUMBER = 292020;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);

// test here
function foreachbug()
{
    var arryOuter = ["outervalue0", "outervalue1"];
    var arryInner = ["innervalue1","innervalue2"];

    for (var j in arryOuter)
    {
        var result = (j in arryOuter);
        if (!result)
        {
            return ("enumerated property not in object: (" +
                    j + " in  arryOuter) " + result);
            return result;
        }


        for each (k in arryInner)
        {
            // this for-each-in should not affect the outer for-in
        }
    }
    return '';
}

reportCompare('', foreachbug());
