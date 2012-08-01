// |reftest| pref(javascript.options.xml.content,true) skip-if(!xulRuntime.shell) -- does not always dismiss alert
/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Regress - Do not crash on gc";
var BUGNUMBER = 292455;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

function output (text)
{
    if (typeof alert != 'undefined')
    {
        alert(text);
    }
    else if (typeof print != 'undefined')
    {
        print(text);
    }
}

function doTest ()
{
    var html = <div xml:lang="en">
        <h1>Kibology for all</h1>
        <p>Kibology for all. All for Kibology. </p>
        </div>;
    // insert new child as last child
    html.* += <h1>All for Kibology</h1>;
    gc();
    output(html);
    html.* += <p>All for Kibology. Kibology for all.</p>;
    gc();
    output(html);
}

doTest();

TEST(1, expect, actual);

END();
