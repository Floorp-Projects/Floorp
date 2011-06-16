/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Andreas Gal <gal@mozilla.com>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 547941;
var summary = 'js weak maps';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
    enterFunc ('test');
    printBugNumber(BUGNUMBER);
    printStatus(summary);

    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, todo) {
        let thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = true;
        }

        check(function() thrown, todo);
    }

    var key = {};
    var map = WeakMap();

    check(function() !map.has(key));
    map.set(key, 42);
    check(function() map.get(key) == 42);
    check(function() typeof map.get({}) == "undefined");
    check(function() map.get({}, "foo") == "foo");

    gc(); gc(); gc();

    check(function() map.get(key) == 42);
    map.delete(key);
    check(function() typeof map.get(key) == "undefined");
    check(function() !map.has(key));

    var value = { };
    map.set(new Object(), value);
    gc(); gc(); gc();

    print ("done");

    reportCompare(0, TestFailCount, "weak map tests");

    exitFunc ('test');
}
