// |reftest| skip -- bug 528404 - disable due to random timeouts
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'No Syntax Error when trailing space and XML.ignoreWhitespace ' +
    'true';
var BUGNUMBER = 324688;
var actual = 'No Error';
var expect = 'No Error';
var t; // use global scope to prevent timer from being GC'ed
START(summary);

function init()
{
    if (typeof Components != 'undefined')
    {
        try
        {
            netscape.security.PrivilegeManager.
                enablePrivilege('UniversalXPConnect');

            var TestObject = {
            observe: function () {
                    try
                    {
                        printBugNumber(BUGNUMBER);
                        printStatus (summary);
                        printStatus('Browser only: requires UniversalXPConnect');

                        printStatus("XML.ignoreWhitespace=" +
                                    XML.ignoreWhitespace);
                        var x = new XML("<a></a> ");
                    }
                    catch(ex2)
                    {
                        actual = ex2 + '';
                    }
                    print('expect = ' + expect);
                    print('actual = ' + actual);
                    TEST(1, expect, actual);
                    END();
                    gDelayTestDriverEnd = false;
                    jsTestDriverEnd();
                }
            };

            t = Components.classes["@mozilla.org/timer;1"].
                createInstance(Components.interfaces.nsITimer);
            t.init(TestObject, 100, t.TYPE_ONE_SHOT);
        }
        catch(ex)
        {
            printStatus('Requires UniversalXPConnect');
        }
    }
}

if (typeof window != 'undefined')
{
    // delay test driver end
    gDelayTestDriverEnd = true;

    window.addEventListener("load", init, false);
}
else
{
    TEST(1, expect, actual);
    END();
}

